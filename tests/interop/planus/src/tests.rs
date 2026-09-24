use dexkit_rs::{encoding, service::AnalysisService};
use serde_json::{json, Value};
use std::path::PathBuf;

unsafe extern "C" {
    fn dexkit_reference_matches(
        dex: *const u8,
        dex_size: usize,
        text: *const u8,
        text_size: usize,
    ) -> i32;
}
fn fixture() -> PathBuf {
    std::env::var_os("DEXKIT_PLANUS_FIXTURE")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("target/fixture/fixture.dex")
        })
}
struct Test {
    service: AnalysisService,
    instance: String,
}
impl Test {
    fn new(unicode: bool) -> Self {
        let mut path = fixture();
        if unicode {
            path.set_file_name("unicode.dex");
        }
        assert!(
            path.is_file(),
            "Assemble fixtures with run.py before testing"
        );
        let mut service = AnalysisService::new(vec![path.parent().unwrap().into()]).unwrap();
        let opened = ok(service.call("dexkit_open", json!({"path":path})));
        Self {
            service,
            instance: opened["instanceId"].as_str().unwrap().into(),
        }
    }
    fn call(&mut self, tool: &str, mut args: Value) -> Value {
        args["instanceId"] = json!(self.instance);
        ok(self.service.call(&format!("dexkit_{tool}"), args))
    }
    fn methods(&mut self, matcher: Value) -> Value {
        self.call("find_methods", json!({"query":{"matcher":matcher}}))
    }
    fn method(&mut self, name: &str) -> String {
        let rows = self.methods(named(name));
        assert_eq!(count(&rows), 1);
        rows["items"][0]["entityId"].as_str().unwrap().into()
    }
}
fn ok(result: Value) -> Value {
    assert_eq!(result["ok"], true, "{result}");
    result["data"].clone()
}
fn count(result: &Value) -> usize {
    result["resultSet"]["totalItems"]
        .as_str()
        .unwrap()
        .parse()
        .unwrap()
}
fn text(value: &str) -> Value {
    json!({"value":value,"match":"equal"})
}
fn named(value: &str) -> Value {
    json!({"methodName":text(value)})
}

#[test]
fn numeric_union_variants_and_native_bits() {
    let mut t = Test::new(false);
    let wanted = json!([
        {"type":"int8","value":7}, {"type":"int16","value":8}, {"type":"int32","value":-9},
        {"type":"int64","value":"9223372036854775807"}, {"type":"float32","value":1.5}, {"type":"float64","value":2.5}
    ]);
    let missing = json!([6, 9, -10, "9223372036854775806", 1.75, 2.75]);
    let query = |values| json!({"methodName":text("numbers"),"usingNumbers":values});
    assert_eq!(count(&t.methods(query(wanted.clone()))), 1);
    for i in 0..6 {
        let mut values = wanted.clone();
        values[i]["value"] = missing[i].clone();
        assert_eq!(
            count(&t.methods(query(values))),
            0,
            "variant {i} was discarded"
        );
    }
    let target = t.method("bits");
    let bits = t.call(
        "describe",
        json!({"entityId":target,"include":["usingNumbers"]}),
    );
    let values = bits["usingNumbers"].as_array().unwrap();
    assert_eq!(values.len(), 4);
    for expected in [
        "0x8000000000000000",
        "0x7ff8000000001234",
        "0xfff0000000000000",
        "0x0000000080000000",
    ] {
        assert!(
            values.iter().any(|n| n["rawBits"] == expected),
            "{expected}"
        );
    }
}

#[test]
fn nested_annotation_union_vector_changes_results() {
    let mut t = Test::new(false);
    let matcher = |number| {
        json!({
            "className":text("interop.Probe"),
            "annotations":{"annotations":[{
                "annotationType":{"className":text("interop.Marker")},
                "elements":{"elements":[
                    {"name":text("value"),"value":{"type":"array","value":{"matchType":"equal","values":[
                        {"type":"int32","value":number},{"type":"int64","value":"9223372036854775807"},
                        {"type":"string","value":text("marker")},{"type":"bool","value":true}
                    ]}}},
                    {"name":text("child"),"value":{"type":"annotation","value":{
                        "annotationType":{"className":text("interop.Child")},
                        "elements":{"elements":[{"name":text("enabled"),"value":{"type":"bool","value":true}}]}
                    }}}
                ]}
            }]}
        })
    };
    assert_eq!(
        count(&t.call("find_classes", json!({"query":{"matcher":matcher(7)}}))),
        1
    );
    assert_eq!(
        count(&t.call("find_classes", json!({"query":{"matcher":matcher(8)}}))),
        0
    );
}

#[test]
fn boolean_groups_calls_and_direct_relations() {
    let mut t = Test::new(false);
    let matcher = json!({"anyOf":[named("target"),named("empty")],"noneOf":[named("empty")],
        "allOf":[{"usingStrings":[text("token")]}]});
    let result = t.methods(matcher);
    assert_eq!(count(&result), 1);
    let target = result["items"][0]["entityId"].clone();
    let callers = t.call("relations", json!({"entityId":target,"relation":"callers"}));
    assert_eq!(count(&callers), 1);
    assert_eq!(
        callers["items"][0]["descriptor"],
        "Linterop/Probe;->caller()V"
    );
    assert_eq!(
        count(&t.methods(json!({"invokingMethods":{"methods":[named("target")]}}))),
        1
    );
    assert_eq!(
        count(&t.methods(json!({"invokingMethods":{"methods":[named("strings")]}}))),
        0
    );
}

#[test]
fn scopes_empty_results_and_parameter_wildcards() {
    let mut t = Test::new(false);
    let all = t.call("find_methods", json!({"query":{}}));
    assert_eq!(count(&all), 7);
    let empty = t.call(
        "find_methods",
        json!({"query":{"scope":{"within":{"entityIds":[]}}}}),
    );
    assert_eq!(count(&empty), 0);
    let again = t.call(
        "find_methods",
        json!({"query":{"scope":{"within":{"resultSetId":empty["resultSet"]["id"]}}}}),
    );
    assert_eq!(count(&again), 0);
    let parameter = json!({"parameterType":{"className":text("java.lang.String")}});
    assert_eq!(
        count(&t.methods(json!({"parameters":{"parameters":[null,parameter]}}))),
        1
    );
    assert_eq!(
        count(&t.methods(json!({"parameters":{"parameters":[parameter,null]}}))),
        0
    );
    assert_eq!(
        count(&t.methods(json!({"parameters":{"parameters":[]}}))),
        6
    );
}

#[test]
fn normal_unicode_matches_independent_cpp_producer() {
    let mut t = Test::new(false);
    let dex = std::fs::read(fixture()).unwrap();
    for value in ["ascii", "bmp\u{20ac}", "nul\0end", "pair\u{1f600}"] {
        let bytes = encoding::encode(value);
        // SAFETY: valid fixture and borrowed text are live for the synchronous
        // independent C++ builder/Core call. No pointer is retained.
        let expected = unsafe {
            dexkit_reference_matches(dex.as_ptr(), dex.len(), bytes.as_ptr(), bytes.len())
        };
        assert_eq!(expected, 1);
        assert_eq!(
            count(&t.methods(json!({"methodName":text("strings"),"usingStrings":[text(value)]}))),
            expected as usize
        );
    }
    let value = b"pair\xf0\x9f\x98\x80";
    // The reference confirms UTF-8 bytes themselves still do not match Core.
    assert_eq!(
        unsafe { dexkit_reference_matches(dex.as_ptr(), dex.len(), value.as_ptr(), value.len()) },
        0
    );
}

#[test]
fn native_unicode_results_decode_and_lone_surrogates_fail_explicitly() {
    let mut t = Test::new(true);
    let classes = t.call("find_classes", json!({"query":{}}));
    assert_eq!(
        classes["items"][0]["source"]["sourceFile"],
        "source\0\u{1f600}"
    );
    let metadata = t.call(
        "describe",
        json!({"entityId":classes["items"][0]["entityId"],"include":["annotations"]}),
    );
    let elements = metadata["annotations"][0]["elements"].as_array().unwrap();
    let scalar = elements.iter().find(|e| e["name"] == "scalar").unwrap();
    assert_eq!(scalar["value"]["value"], "nul\0pair\u{1f600}");
    for (name, kind, bits) in [
        ("negativeFloat", "float32Bits", "0x80000000"),
        ("positiveFloat", "float32Bits", "0x00000000"),
        ("negativeDouble", "float64Bits", "0x8000000000000000"),
        ("positiveDouble", "float64Bits", "0x0000000000000000"),
    ] {
        let element = elements.iter().find(|e| e["name"] == name).unwrap();
        assert_eq!(
            element["value"],
            json!({"type":kind,"value":bits}),
            "{name}"
        );
    }
    let method = t.method("strings");
    let strings = t.call(
        "describe",
        json!({"entityId":method,"include":["usingStrings"]}),
    );
    assert!(strings["usingStrings"]
        .as_array()
        .unwrap()
        .contains(&json!("nul\0end")));
    let mut raw = Test::new(false);
    let target = raw.method("strings");
    let response = raw.service.call(
        "dexkit_describe",
        json!({"instanceId":raw.instance,"entityId":target,"include":["usingStrings"]}),
    );
    assert_eq!(response["ok"], false);
    assert_eq!(response["error"]["code"], "STRING_ENCODING");
    assert!(serde_json::from_str::<Value>(r#"{"value":"\ud800"}"#).is_err());
}

#[test]
fn stable_paging_ownership_and_closed_handles() {
    let mut t = Test::new(false);
    let page = t.call("find_methods", json!({"query":{},"pageSize":2}));
    assert_eq!(count(&page), 7);
    assert_eq!(page["items"].as_array().unwrap().len(), 2);
    let cursor = page["page"]["nextCursor"].clone();
    let next = t.call("page", json!({"cursor":cursor}));
    let repeated = t.call("page", json!({"cursor":cursor}));
    assert_eq!(next["items"], repeated["items"]);
    let wrong = ok(t.service.call("dexkit_open", json!({"path":fixture()})))["instanceId"].clone();
    let response = t.service.call(
        "dexkit_describe",
        json!({"instanceId":wrong,"entityId":page["items"][0]["entityId"]}),
    );
    assert_eq!(response["error"]["code"], "INVALID_ENTITY");
    t.call("close", json!({}));
    let response = t.service.call(
        "dexkit_page",
        json!({"instanceId":t.instance,"cursor":cursor}),
    );
    assert_eq!(response["error"]["code"], "INSTANCE_CLOSED");
    assert_eq!(
        page["items"].as_array().unwrap().len(),
        2,
        "owned output survives close"
    );
}

#[test]
fn smali_artifact_and_limit_errors() {
    let mut t = Test::new(false);
    let method = t.method("strings");
    let inline = t.call("smali", json!({"entityId":method}));
    assert!(inline["text"].as_str().unwrap().contains("\\ud800"));
    let artifact = t.call("smali", json!({"entityId":method,"delivery":"artifact"}));
    let a = artifact["artifact"]["id"].clone();
    let mut bytes = String::new();
    let mut start = 0;
    loop {
        let chunk = t.call(
            "read_artifact",
            json!({"artifactId":a,"startByte":start,"maxBytes":31}),
        );
        bytes.push_str(chunk["text"].as_str().unwrap());
        match chunk["nextByte"].as_u64() {
            Some(next) => start = next,
            None => break,
        }
    }
    assert_eq!(bytes, inline["text"].as_str().unwrap());
    let limited = t.service.call(
        "dexkit_smali",
        json!({"instanceId":t.instance,"entityId":method,"maxOutputBytes":4}),
    );
    assert_eq!(limited["ok"], false);
    assert_eq!(limited["error"]["native"]["error"], 6);
}

#[test]
fn strict_contract_does_not_silently_drop_conditions() {
    let mut t = Test::new(false);
    for query in [
        json!({"matcher":{"anyOf":[]}}),
        json!({"matcher":{"unknown":true}}),
        json!({"matcher":{"methodName":{"value":"strings","match":"regex"}}}),
        json!({"matcher":{"parameters":null}}),
        json!({"scope":{"within":{"entityIds":[],"resultSetId":"x"}}}),
    ] {
        let result = t.service.call(
            "dexkit_find_methods",
            json!({"instanceId":t.instance,"query":query}),
        );
        assert_eq!(result["ok"], false, "{result}");
        assert_eq!(result["error"]["category"], "invalid_request");
    }
    assert_eq!(count(&t.methods(named("target"))), 1);
}

#[test]
fn fields_flags_opcodes_and_using_field_matchers() {
    let mut t = Test::new(false);
    let field = t.call(
        "find_fields",
        json!({"query":{"matcher":{
            "fieldName":text("counter"),"accessFlags":{"flags":9,"match":"equal"}
        }}}),
    );
    assert_eq!(count(&field), 1);
    assert_eq!(
        field["items"][0]["descriptor"],
        "Linterop/Probe;->counter:I"
    );
    let readers = t.call(
        "relations",
        json!({"entityId":field["items"][0]["entityId"],"relation":"fieldReaders"}),
    );
    assert_eq!(
        readers["items"][0]["descriptor"],
        "Linterop/Probe;->target()V"
    );
    assert_eq!(
        count(&t.methods(
            json!({"usingFields":[{"field":{"fieldName":text("counter")},"usingType":"put"}]})
        )),
        1
    );
    assert_eq!(
        count(&t.methods(
            json!({"usingFields":[{"field":{"fieldName":text("absent")},"usingType":"put"}]})
        )),
        0
    );
    assert_eq!(
        count(&t.methods(
            json!({"methodName":text("target"),"opCodes":{"opCodes":[26],"matchType":"startWith"}})
        )),
        1
    );
    assert_eq!(
        count(&t.methods(
            json!({"methodName":text("target"),"opCodes":{"opCodes":[14],"matchType":"startWith"}})
        )),
        0
    );
}

#[test]
fn declared_methods_do_not_include_undefined_references() {
    let path = fixture().with_file_name("references.dex");
    let mut service = AnalysisService::new(vec![path.parent().unwrap().into()]).unwrap();
    let instance = ok(service.call("dexkit_open", json!({"path":path})))["instanceId"].clone();
    let classes = ok(service.call(
        "dexkit_find_classes",
        json!({"instanceId":instance,"query":{}}),
    ));
    let entity = classes["items"][0]["entityId"].clone();
    let members = ok(service.call(
        "dexkit_relations",
        json!({"instanceId":instance,"entityId":entity,"relation":"declaredMethods"}),
    ));
    assert_eq!(count(&members), 1);
    assert_eq!(
        members["items"][0]["descriptor"],
        "Linterop/References;->touch()V"
    );
    let all = ok(service.call(
        "dexkit_find_methods",
        json!({"instanceId":instance,"query":{}}),
    ));
    assert_eq!(
        count(&all),
        2,
        "Core query also includes referenced IDs in a defined class"
    );
    let fields = ok(service.call(
        "dexkit_relations",
        json!({"instanceId":instance,"entityId":entity,"relation":"fieldReferences"}),
    ));
    assert_eq!(
        count(&fields),
        2,
        "This relation explicitly includes field references"
    );
}
