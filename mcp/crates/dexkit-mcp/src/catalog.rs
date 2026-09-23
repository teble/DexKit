use dexkit_rs::api::*;
use rmcp::model::{Tool, ToolAnnotations};
use schemars::JsonSchema;
use serde_json::{json, Value};
use std::sync::Arc;

pub fn tools() -> Vec<Tool> {
    vec![
        tool::<Empty,Capabilities>("capabilities","Report the actual API contract, supported tools, lifetime/size limits and native execution limitations."),
        tool::<Open,Opened>("open","Open a local APK/DEX under an allowed directory as an immutable snapshot. Return an instanceId and SHA-256 fingerprint. Close unused instances."),
        tool::<Instance,Closed>("close","Close an instance and invalidate all its entities, result sets and temporary artifacts. Input files are never modified."),
        tool::<FindClasses,Found>("find_classes","Find classes with typed matchers. className uses Java names (example java.lang.String). Omitted conditions do not constrain; boolean arrays must be nonempty. Results are complete, sorted by descriptor and DEX identity, then paginated in the adapter."),
        tool::<FindMethods,Found>("find_methods","Find methods using strings, numbers, annotations, flags, parameters and direct call relationships. Text match requires explicit equal/contains/startWith/endWith (no regex). Parameter null slots are positional wildcards. Int64 values use decimal strings; float queries use Core's numeric comparison, not bit equality. pageSize does not limit native evaluation."),
        tool::<FindFields,Found>("find_fields","Find fields with typed names, types, flags, annotations and reader/writer matchers. All conditions combine with AND. scope.within accepts same-instance entityIds or a complete resultSetId of the same kind."),
        tool::<Describe,Described>("describe","Read entity metadata and explicitly requested annotations or method code details. Code details are method-only. Numeric literals return opcode and raw bits. Unrepresentable DEX text is an error, never replacement text."),
        tool::<Relations,Found>("relations","Find direct invokes/callers for a method, readers/writers for a field, or declared methods/field references for a class. This does not compute transitive call paths."),
        tool::<Page,Found>("page","Read the next page using an opaque cursor from this instance. Does not rerun the native query. Cursors expire or become invalid when the instance closes."),
        tool::<Smali,SmaliOutput>("smali","Disassemble a class or method. inline is limited to 64 KiB; artifact publishes a managed dexkit:// resource. maxOutputBytes limits generation, not a truncated preview. None/strict debug modes follow the native writer contract."),
        tool::<ReadArtifact,ArtifactChunk>("read_artifact","Read a bounded UTF-8 chunk of a managed artifact, using byte offsets from nextByte. Does not accept arbitrary filesystem paths. maxBytes is 1..65536."),
    ]
}
fn tool<I: JsonSchema, O: JsonSchema>(name: &str, description: &str) -> Tool {
    let mut input = serde_json::to_value(schemars::schema_for!(I)).unwrap();
    strict_properties(&mut input);
    let mut output = serde_json::to_value(schemars::schema_for!(Reply<O>)).unwrap();
    output["type"] = json!("object");
    Tool::new(
        format!("dexkit_v1_{name}"),
        description.to_owned(),
        input.as_object().unwrap().clone(),
    )
    .with_raw_output_schema(Arc::new(output.as_object().unwrap().clone()))
    .with_annotations(
        ToolAnnotations::new()
            .read_only(!matches!(name, "open" | "close" | "smali"))
            .destructive(false)
            .open_world(false)
            .idempotent(matches!(
                name,
                "capabilities" | "describe" | "page" | "read_artifact"
            )),
    )
}
// Serde Option accepts null, but the public contract requires omitted optional
// properties. Preserve nullable array items, which encode parameter wildcards.
// The service enforces the same rule before deserializing.
fn strict_properties(schema: &mut Value) {
    match schema {
        Value::Object(map) => {
            if let Some(properties) = map.get_mut("properties").and_then(Value::as_object_mut) {
                for (name, property) in properties {
                    if let Some(variants) = property.get_mut("anyOf").and_then(Value::as_array_mut)
                    {
                        variants.retain(|v| v.get("type") != Some(&json!("null")));
                        if variants.len() == 1 {
                            let replacement = variants[0].as_object().cloned().unwrap_or_default();
                            let object = property.as_object_mut().unwrap();
                            object.remove("anyOf");
                            object.extend(replacement);
                        }
                    }
                    if let Some(types) = property.get_mut("type").and_then(Value::as_array_mut) {
                        types.retain(|t| t != "null");
                        if types.len() == 1 {
                            property["type"] = types[0].clone();
                        }
                    }
                    if property.get("default") == Some(&Value::Null) {
                        property.as_object_mut().unwrap().remove("default");
                    }
                    if ["allOf", "anyOf", "noneOf"].contains(&name.as_str()) {
                        property["minItems"] = json!(1);
                    }
                    if name == "pageSize" {
                        property["minimum"] = json!(1);
                        property["maximum"] = json!(500);
                    }
                    if name == "maxBytes" {
                        property["minimum"] = json!(1);
                        property["maximum"] = json!(65536);
                    }
                    if name == "maxOutputBytes" {
                        property["minimum"] = json!(1);
                        property["maximum"] = json!(16777216);
                    }
                }
            }
            for value in map.values_mut() {
                strict_properties(value);
            }
        }
        Value::Array(items) => {
            for item in items {
                strict_properties(item);
            }
        }
        _ => (),
    }
}
