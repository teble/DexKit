//! Read-only views of the exact query input schemas advertised by the parent.
use dexkit_rs::{api, error::Error};
use rmcp::model::Tool;
use schemars::JsonSchema;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};

pub const NAME: &str = "dexkit_get_query_schema";
pub const MAX_REPLY: usize = 16 * 1024;
pub const MAX_MCP_REPLY: usize = 64 * 1024;
const MAX_LINKS: usize = 64;

#[derive(Clone, Copy, Debug, Deserialize, Serialize, JsonSchema)]
#[schemars(inline)]
pub enum QueryTool {
    #[serde(rename = "dexkit_find_classes")]
    Classes,
    #[serde(rename = "dexkit_find_methods")]
    Methods,
    #[serde(rename = "dexkit_find_fields")]
    Fields,
}
impl QueryTool {
    pub fn name(self) -> &'static str {
        match self {
            Self::Classes => "dexkit_find_classes",
            Self::Methods => "dexkit_find_methods",
            Self::Fields => "dexkit_find_fields",
        }
    }
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct GetQuerySchema {
    pub tool: QueryTool,
    /// Omit for overview; empty string reads the full document. Copy other pointers from links.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[schemars(extend("maxLength" = 1024))]
    pub pointer: Option<String>,
    /// Optional precondition, copied from schemaHash in an earlier response.
    #[serde(skip_serializing_if = "Option::is_none")]
    #[schemars(extend("maxLength" = 71))]
    pub if_schema_hash: Option<String>,
}
#[derive(Clone, Debug, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct DiscoveryCapability {
    pub tool: &'static str,
    pub execution: &'static str,
    pub requires_instance: bool,
    pub discovery_version: u32,
}
impl Default for DiscoveryCapability {
    fn default() -> Self {
        Self {
            tool: NAME,
            execution: "parent",
            requires_instance: false,
            discovery_version: 1,
        }
    }
}
#[derive(Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct McpCapabilities {
    #[serde(flatten)]
    pub analysis: api::Capabilities,
    pub query_schema_discovery: DiscoveryCapability,
}
#[derive(Clone, Debug, Serialize, JsonSchema)]
pub struct Link {
    pub relation: String,
    pub pointer: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub label: Option<String>,
}
#[derive(Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct DiscoveryData {
    pub tool: QueryTool,
    pub api_major: u32,
    pub contract_revision: &'static str,
    pub discovery_version: u32,
    pub schema_hash: String,
    pub kind: &'static str,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pointer: Option<String>,
    pub standalone: bool,
    pub reference_context: &'static str,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub fragment: Option<Value>,
    pub links: Vec<Link>,
    pub links_complete: bool,
    pub arguments: Vec<Value>,
    pub examples: Vec<Value>,
    pub notes: Vec<&'static str>,
    pub read_full: GetQuerySchema,
}
#[derive(Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct DiscoveryError {
    #[serde(flatten)]
    pub error: Error,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub retry: Option<GetQuerySchema>,
    pub links: Vec<Link>,
    pub links_complete: bool,
}
#[derive(Serialize, JsonSchema)]
#[serde(untagged)]
pub enum DiscoveryReply {
    Success {
        #[schemars(extend("const" = true))]
        ok: bool,
        data: DiscoveryData,
    },
    Failure {
        #[schemars(extend("const" = false))]
        ok: bool,
        error: DiscoveryError,
    },
}
struct Document {
    schema: Value,
    hash: String,
    self_contained: bool,
}
pub struct QueryContracts {
    documents: BTreeMap<String, Document>,
}
impl QueryContracts {
    pub fn new(tools: &[Tool]) -> Self {
        let documents = tools
            .iter()
            .filter(|t| t.name.starts_with("dexkit_find_"))
            .map(|tool| {
                let schema = Value::Object(tool.input_schema.as_ref().clone());
                let bytes = serde_json::to_vec(&sorted(&schema)).expect("schema JSON");
                let hash = format!("sha256:{:x}", Sha256::digest(bytes));
                let self_contained = local_refs_resolve(&schema, &schema);
                (
                    tool.name.to_string(),
                    Document {
                        schema,
                        hash,
                        self_contained,
                    },
                )
            })
            .collect();
        Self { documents }
    }
    pub fn call(&self, arguments: Value) -> Value {
        let request = dexkit_rs::service::validate_json(&arguments).and_then(|()| {
            serde_json::from_value::<GetQuerySchema>(arguments)
                .map_err(|e| Error::invalid(e.to_string()))
        });
        let request = match request {
            Ok(r) => r,
            Err(e) => return failure(e, None, vec![], true),
        };
        let document = &self.documents[request.tool.name()];
        let retry = GetQuerySchema {
            tool: request.tool,
            pointer: None,
            if_schema_hash: None,
        };
        if request
            .if_schema_hash
            .as_ref()
            .is_some_and(|h| h.len() > 71)
        {
            return failure(
                Error::invalid("ifSchemaHash exceeds 71 bytes"),
                Some(retry),
                vec![],
                true,
            );
        }
        if request
            .if_schema_hash
            .as_ref()
            .is_some_and(|h| h != &document.hash)
        {
            return failure(
                Error::new(
                    "invalid_request",
                    "SCHEMA_CHANGED",
                    "Schema changed; read the overview again",
                ),
                Some(retry),
                vec![],
                true,
            );
        }
        let node = match request.pointer.as_deref() {
            None => &document.schema,
            Some(pointer) => {
                if !valid_pointer(pointer) {
                    return failure(Error::new("invalid_request", "SCHEMA_POINTER_INVALID", "Expected a returned JSON Pointer (at most 1024 bytes); omit pointer for overview"), Some(retry), vec![], true);
                }
                match document.schema.pointer(pointer) {
                    Some(node) => node,
                    None => {
                        return failure(
                            Error::new(
                                "invalid_request",
                                "SCHEMA_POINTER_INVALID",
                                "Pointer does not exist; read the overview and follow links",
                            ),
                            Some(retry),
                            vec![],
                            true,
                        )
                    }
                }
            }
        };
        let full = request.pointer.as_deref() == Some("");
        let overview = request.pointer.is_none();
        let mut links = if full {
            vec![]
        } else {
            links(
                node,
                request.pointer.as_deref().unwrap_or(""),
                &document.schema,
            )
        };
        if overview {
            links.retain(|link| link.relation != "$defs" && link.relation != "definitions");
            if let Some(query) = document.schema.pointer("/properties/query") {
                let (query, pointer) = dereference(query, "/properties/query", &document.schema);
                links.extend(links_for_properties(query, &pointer));
            }
            links.push(Link {
                relation: "full".into(),
                pointer: "".into(),
                label: Some("Complete input schema".into()),
            });
        }
        let links_complete = links.len() <= MAX_LINKS;
        links.truncate(MAX_LINKS);
        let data = DiscoveryData {
            tool: request.tool, api_major: api::API_MAJOR, contract_revision: api::CONTRACT_REVISION,
            discovery_version: 1, schema_hash: document.hash.clone(),
            kind: if overview { "overview" } else if full { "full" } else { "fragment" },
            pointer: request.pointer.clone(), standalone: full && document.self_contained,
            reference_context: "All fragment $refs resolve against this tool's full input schema, not this reply. JSON Schema does not include every runtime semantic check.",
            fragment: if overview { None } else { Some(node.clone()) },
            links, links_complete,
            arguments: if overview { overview_arguments(&document.schema) } else { vec![] },
            examples: if overview { examples(request.tool) } else { vec![] },
            notes: if overview || full { notes() } else { vec![] },
            read_full: GetQuerySchema { tool: request.tool, pointer: Some("".into()), if_schema_hash: Some(document.hash.clone()) },
        };
        let reply = serde_json::to_value(DiscoveryReply::Success { ok: true, data })
            .expect("discovery JSON");
        if serde_json::to_vec(&reply).expect("discovery JSON").len() > MAX_REPLY {
            let mut children = links_for_children(node, request.pointer.as_deref().unwrap_or(""));
            let complete = children.len() <= 16;
            children.truncate(16);
            return failure(Error::new("resource_limit", "SCHEMA_SECTION_TOO_LARGE", "Section exceeds the 16 KiB help budget; read a child pointer instead. JSON was not truncated."), Some(retry), children, complete);
        }
        reply
    }
}
pub fn failure(
    error: Error,
    retry: Option<GetQuerySchema>,
    links: Vec<Link>,
    links_complete: bool,
) -> Value {
    let mut reply = DiscoveryReply::Failure {
        ok: false,
        error: DiscoveryError {
            error,
            retry,
            links,
            links_complete,
        },
    };
    loop {
        let value = serde_json::to_value(&reply).expect("discovery error JSON");
        if serde_json::to_vec(&value)
            .expect("discovery error JSON")
            .len()
            <= MAX_REPLY
        {
            return value;
        }
        let DiscoveryReply::Failure { error, .. } = &mut reply else {
            unreachable!()
        };
        if error.links.pop().is_none() {
            error.retry = None;
            error.error = Error::limit("Help error exceeds reply budget");
        }
        error.links_complete = false;
    }
}
fn local_refs_resolve(node: &Value, root: &Value) -> bool {
    match node {
        Value::Object(map) => {
            let reference_ok = map.get("$ref").is_none_or(|reference| {
                reference
                    .as_str()
                    .and_then(|r| r.strip_prefix('#'))
                    .is_some_and(|p| valid_pointer(p) && root.pointer(p).is_some())
            });
            reference_ok && map.values().all(|value| local_refs_resolve(value, root))
        }
        Value::Array(values) => values.iter().all(|value| local_refs_resolve(value, root)),
        _ => true,
    }
}
fn sorted(value: &Value) -> Value {
    match value {
        Value::Object(map) => Value::Object(
            map.iter()
                .collect::<BTreeMap<_, _>>()
                .into_iter()
                .map(|(k, v)| (k.clone(), sorted(v)))
                .collect(),
        ),
        Value::Array(values) => Value::Array(values.iter().map(sorted).collect()),
        _ => value.clone(),
    }
}
fn valid_pointer(pointer: &str) -> bool {
    if pointer.len() > 1024 || (!pointer.is_empty() && !pointer.starts_with('/')) {
        return false;
    }
    let mut chars = pointer.chars();
    while let Some(ch) = chars.next() {
        if ch == '~' && !matches!(chars.next(), Some('0' | '1')) {
            return false;
        }
    }
    true
}
fn child(pointer: &str, name: &str) -> String {
    format!("{pointer}/{}", name.replace('~', "~0").replace('/', "~1"))
}
fn links_for_properties(node: &Value, pointer: &str) -> Vec<Link> {
    node.get("properties")
        .and_then(Value::as_object)
        .map(|properties| {
            properties
                .keys()
                .map(|name| Link {
                    relation: "property".into(),
                    pointer: child(&child(pointer, "properties"), name),
                    label: Some(name.clone()),
                })
                .collect()
        })
        .unwrap_or_default()
}
fn links_for_children(node: &Value, pointer: &str) -> Vec<Link> {
    match node {
        Value::Object(map) => map
            .iter()
            .filter(|(_, v)| v.is_object() || v.is_array())
            .map(|(name, _)| Link {
                relation: "child".into(),
                pointer: child(pointer, name),
                label: Some(name.clone()),
            })
            .collect(),
        Value::Array(values) => values
            .iter()
            .enumerate()
            .map(|(i, _)| Link {
                relation: "item".into(),
                pointer: child(pointer, &i.to_string()),
                label: None,
            })
            .collect(),
        _ => vec![],
    }
}
fn links(node: &Value, pointer: &str, root: &Value) -> Vec<Link> {
    let mut result = links_for_properties(node, pointer);
    for table in ["$defs", "definitions"] {
        if let Some(defs) = node.get(table).and_then(Value::as_object) {
            result.extend(defs.keys().map(|key| Link {
                relation: table.into(),
                pointer: child(&child(pointer, table), key),
                label: Some(key.clone()),
            }));
        }
    }
    for keyword in [
        "items",
        "additionalProperties",
        "not",
        "if",
        "then",
        "else",
        "contains",
    ] {
        if node
            .get(keyword)
            .is_some_and(|v| v.is_object() || v.is_boolean())
        {
            result.push(Link {
                relation: keyword.into(),
                pointer: child(pointer, keyword),
                label: None,
            });
        }
    }
    for keyword in ["oneOf", "anyOf", "allOf", "prefixItems"] {
        if let Some(branches) = node.get(keyword).and_then(Value::as_array) {
            result.extend(branches.iter().enumerate().map(|(i, b)| {
                Link {
                    relation: keyword.into(),
                    pointer: child(&child(pointer, keyword), &i.to_string()),
                    label: b
                        .pointer("/properties/type/const")
                        .and_then(Value::as_str)
                        .map(str::to_owned),
                }
            }));
        }
    }
    // Inspect syntax only. Never follow references recursively or fetch URLs.
    fn refs(node: &Value, found: &mut BTreeSet<String>) {
        match node {
            Value::Object(map) => {
                if let Some(reference) = map.get("$ref").and_then(Value::as_str) {
                    if let Some(pointer) = reference.strip_prefix('#') {
                        found.insert(pointer.to_owned());
                    }
                }
                for (key, value) in map {
                    if key != "$defs" && key != "definitions" {
                        refs(value, found);
                    }
                }
            }
            Value::Array(array) => {
                for value in array {
                    refs(value, found);
                }
            }
            _ => (),
        }
    }
    let mut found = BTreeSet::new();
    refs(node, &mut found);
    result.extend(
        found
            .into_iter()
            .filter(|p| valid_pointer(p) && root.pointer(p).is_some())
            .map(|pointer| Link {
                relation: "$ref".into(),
                pointer,
                label: None,
            }),
    );
    if result.is_empty() {
        result = links_for_children(node, pointer);
    }
    result
}
fn dereference<'a>(mut node: &'a Value, pointer: &str, root: &'a Value) -> (&'a Value, String) {
    let mut pointer = pointer.to_owned();
    let mut seen = BTreeSet::new();
    while let Some(target) = node
        .get("$ref")
        .and_then(Value::as_str)
        .and_then(|r| r.strip_prefix('#'))
    {
        if !seen.insert(target) {
            break;
        }
        let Some(next) = root.pointer(target) else {
            break;
        };
        node = next;
        pointer = target.to_owned();
    }
    (node, pointer)
}
fn overview_arguments(root: &Value) -> Vec<Value> {
    let required = root.get("required").and_then(Value::as_array);
    root["properties"].as_object().expect("query properties").iter().map(|(name,value)| {
        let (value, _) = dereference(value,"",root);
        let mut arg = json!({"name":name,"required":required.is_some_and(|r|r.iter().any(|v| v == name)),"pointer":child("/properties",name)});
        for key in ["type","default","minimum","maximum","enum"] {
            if let Some(value) = value.get(key) { arg[key] = value.clone(); }
        }
        if let Some(values) = value.pointer("/items/enum") { arg["values"] = values.clone(); }
        arg
    }).collect()
}
fn notes() -> Vec<&'static str> {
    vec![
        "Use an instanceId returned by open. Replace the instanceId placeholder in examples. Help needs no instance.",
        "Different matcher fields combine with AND. allOf/anyOf/noneOf arrays must be nonempty. className uses Java names; results use DEX descriptors.",
        "String/flags conditions use match; collection modes use matchType and preserve one-to-one matching. Conditions on the same element belong in the same matcher. No regex; empty text only supports equal.",
        "Omit unused optional conditions. parameters.parameters=[] means zero parameters; null entries are positional wildcards, not removable entries. Other optional nulls are rejected.",
        "scope.within uses entityIds or resultSetId, never both; handles must belong to the same instance and entity kind. Empty candidates mean no matches.",
        "Number/annotation unions use their type discriminator. int64 values are decimal strings. Float queries compare finite numeric values, not raw bits. Unknown fields and enum values are rejected.",
        "pageSize only controls returned pages; native queries materialize all matches and cannot be interrupted. Results may include references without a definition.",
    ]
}
pub fn examples(tool: QueryTool) -> Vec<Value> {
    let matcher = match tool {
        QueryTool::Classes => json!({"className":{"value":"interop.Probe","match":"equal"}}),
        QueryTool::Methods => json!({"usingStrings":[{"value":"token","match":"contains"}]}),
        QueryTool::Fields => json!({"fieldName":{"value":"counter","match":"equal"}}),
    };
    let make = |matcher| json!({"instanceId":"<instanceId from open>","query":{"matcher":matcher},"pageSize":20,"select":["descriptor","source"]});
    let mut result = vec![make(matcher)];
    if matches!(tool, QueryTool::Methods) {
        result.push(make(json!({"parameters":{"parameters":[null,{"parameterType":{"className":{"value":"java.lang.String","match":"equal"}}}]}})));
        result.push(make(json!({"invokingMethods":{"matchType":"contains","methods":[{"methodName":{"value":"target","match":"equal"},"declaringClass":{"className":{"value":"interop.Probe","match":"equal"}}}]}})));
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn setup() -> (Vec<Tool>, QueryContracts) {
        let tools = crate::catalog::tools();
        let contracts = QueryContracts::new(&tools);
        (tools, contracts)
    }

    #[test]
    fn published_contracts_fragments_links_and_examples_agree() {
        let (tools, contracts) = setup();
        let help = tools.iter().find(|t| t.name == NAME).unwrap();
        assert!(serde_json::to_vec(&help.input_schema).unwrap().len() < 2048);
        assert!(help.input_schema["properties"]["tool"]["enum"].is_array());
        let output = jsonschema::validator_for(
            &serde_json::to_value(help.output_schema.as_ref().unwrap()).unwrap(),
        )
        .unwrap();
        for kind in [QueryTool::Classes, QueryTool::Methods, QueryTool::Fields] {
            let tool = tools.iter().find(|t| t.name == kind.name()).unwrap();
            let root = Value::Object(tool.input_schema.as_ref().clone());
            let validator = jsonschema::validator_for(&root).unwrap();
            let overview = contracts.call(json!({"tool":kind}));
            assert!(output.is_valid(&overview));
            assert_eq!(overview["data"]["kind"], "overview");
            assert!(overview["data"].get("fragment").is_none());
            for example in overview["data"]["examples"].as_array().unwrap() {
                assert!(validator.is_valid(example), "{example}");
                dexkit_rs::service::validate_json(example).unwrap();
            }
            let full = contracts.call(overview["data"]["readFull"].clone());
            assert!(
                output.is_valid(&full),
                "{:?}",
                output
                    .iter_errors(&full)
                    .map(|e| e.to_string())
                    .collect::<Vec<_>>()
            );
            assert_eq!(full["data"]["fragment"], root);
            assert_eq!(full["data"]["standalone"], true);
            assert!(serde_json::to_vec(&full).unwrap().len() <= MAX_REPLY);
            let mut pointers = vec![
                "/properties/query".to_owned(),
                "/properties/select".to_owned(),
            ];
            pointers.extend(
                root["$defs"]
                    .as_object()
                    .unwrap()
                    .keys()
                    .map(|k| child("/$defs", k)),
            );
            for pointer in pointers {
                let reply = contracts.call(json!({"tool":kind,"pointer":pointer,"ifSchemaHash":full["data"]["schemaHash"]}));
                assert_eq!(reply["ok"], true, "{reply}");
                assert!(output.is_valid(&reply));
                assert_eq!(reply["data"]["fragment"], *root.pointer(&pointer).unwrap());
                assert_eq!(reply["data"]["standalone"], false);
                assert_eq!(reply["data"]["linksComplete"], true);
                for link in reply["data"]["links"].as_array().unwrap() {
                    let target = link["pointer"].as_str().unwrap();
                    assert!(root.pointer(target).is_some(), "{link}");
                }
            }
            let mut old = root.clone();
            old["properties"]["select"]["items"] = json!({"$ref":"#/$defs/Select"});
            let old_validator = jsonschema::validator_for(&old).unwrap();
            for select in [
                json!([]),
                json!(["descriptor", "source"]),
                json!(["unknown"]),
                json!(null),
                json!([7]),
            ] {
                let input = json!({"instanceId":"test","query":{},"select":select});
                assert_eq!(old_validator.is_valid(&input), validator.is_valid(&input));
            }
        }
    }

    #[test]
    fn selectors_hash_preconditions_and_output_budgets_fail_explicitly() {
        let (tools, mut contracts) = setup();
        let help = tools.iter().find(|t| t.name == NAME).unwrap();
        let output = jsonschema::validator_for(
            &serde_json::to_value(help.output_schema.as_ref().unwrap()).unwrap(),
        )
        .unwrap();
        for args in [
            json!({}),
            json!({"tool":"not-a-query"}),
            json!({"tool":"dexkit_find_methods","pointer":null}),
            json!({"tool":"dexkit_find_methods","extra":true}),
        ] {
            let reply = contracts.call(args);
            assert_eq!(reply["ok"], false);
            assert!(output.is_valid(&reply));
        }
        for pointer in [
            "#/$defs/Query",
            "/missing",
            "/~2invalid",
            "query.matcher",
            &"/x".repeat(600),
        ] {
            let reply = contracts.call(json!({"tool":QueryTool::Methods,"pointer":pointer}));
            assert_eq!(reply["error"]["code"], "SCHEMA_POINTER_INVALID");
            assert!(output.is_valid(&reply));
            assert_eq!(contracts.call(reply["error"]["retry"].clone())["ok"], true);
        }
        let stale = contracts.call(json!({"tool":QueryTool::Methods,"ifSchemaHash":"sha256:old"}));
        assert_eq!(stale["error"]["code"], "SCHEMA_CHANGED");
        assert!(output.is_valid(&stale));
        let doc = contracts
            .documents
            .get_mut(QueryTool::Methods.name())
            .unwrap();
        doc.schema["large"] = json!({"child":{"description":"x".repeat(MAX_REPLY)}});
        doc.schema["a/b~c"] = json!([null,{"const":"yes"}]);
        let exact = contracts.call(json!({"tool":QueryTool::Methods,"pointer":"/a~1b~0c/1"}));
        assert_eq!(exact["data"]["fragment"], json!({"const":"yes"}));
        let large = contracts.call(json!({"tool":QueryTool::Methods,"pointer":"/large"}));
        assert_eq!(large["error"]["code"], "SCHEMA_SECTION_TOO_LARGE");
        assert!(output.is_valid(&large));
        assert!(serde_json::to_vec(&large).unwrap().len() <= MAX_REPLY);
        assert_eq!(large["error"]["links"][0]["pointer"], "/large/child");
    }
}
