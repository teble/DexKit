//! Validate real subprocess responses against the schemas sent by tools/list.
use serde_json::Value;
use std::collections::HashMap;
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let path = std::env::args()
        .nth(1)
        .ok_or("Expected captured contract cases")?;
    let capture: Value = serde_json::from_slice(&std::fs::read(path)?)?;
    let mut validators = HashMap::new();
    for tool in capture["tools"].as_array().ok_or("Missing tools")? {
        let name = tool["name"].as_str().unwrap();
        let input = jsonschema::validator_for(&tool["inputSchema"])?;
        let output = jsonschema::validator_for(&tool["outputSchema"])?;
        validators.insert(name, (input, output));
    }
    let cases = capture["cases"].as_array().ok_or("Missing cases")?;
    for (index, case) in cases.iter().enumerate() {
        let name = case["name"].as_str().unwrap();
        let (input, output) = validators.get(name).ok_or("Unknown captured tool")?;
        let expected = case["validInput"].as_bool().unwrap();
        if input.is_valid(&case["arguments"]) != expected {
            return Err(format!(
                "Input schema mismatch in case {index} ({name}): {:?}",
                input
                    .iter_errors(&case["arguments"])
                    .map(|e| e.to_string())
                    .collect::<Vec<_>>()
            )
            .into());
        }
        if !output.is_valid(&case["output"]) {
            return Err(format!(
                "Output schema mismatch in case {index} ({name}): {:?}",
                output
                    .iter_errors(&case["output"])
                    .map(|e| e.to_string())
                    .collect::<Vec<_>>()
            )
            .into());
        }
        let mut inverted = case["output"].clone();
        inverted["ok"] = Value::Bool(!inverted["ok"].as_bool().unwrap());
        if output.is_valid(&inverted) {
            return Err(format!(
                "Output schema accepts inverted success flag in case {index} ({name})"
            )
            .into());
        }
    }
    println!(
        "Validated {} tool schemas and {} real request/result cases",
        validators.len(),
        cases.len()
    );
    Ok(())
}
