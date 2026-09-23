use planus_types::intermediate::{DeclarationKind, TypeKind};
use std::collections::{BTreeMap, BTreeSet};
use std::{env, fs, path::PathBuf};
use syn::visit_mut::{self, VisitMut};

// Planus intentionally models FBS strings as UTF-8. DexKit's legacy wire
// contract instead carries DEX MUTF-8. Adapt type paths in the generated AST,
// not field offsets/layouts or the shared schema. Pin the generator/runtime
// together and fail if its expected representation changes.
#[derive(Default)]
struct WireStrings {
    owned: usize,
    borrowed: usize,
}
impl VisitMut for WireStrings {
    // Diagnostic type names belong to Planus, not to FBS string fields.
    fn visit_impl_item_const_mut(&mut self, _: &mut syn::ImplItemConst) {}
    fn visit_type_path_mut(&mut self, ty: &mut syn::TypePath) {
        let parts: Vec<_> = ty
            .path
            .segments
            .iter()
            .map(|s| s.ident.to_string())
            .collect();
        if parts == ["planus", "alloc", "string", "String"] {
            *ty = syn::parse_quote!(crate::encoding::WireString);
            self.owned += 1;
        } else if parts == ["core", "primitive", "str"] || parts == ["str"] {
            *ty = syn::parse_quote!(crate::encoding::WireStr);
            self.borrowed += 1;
        } else {
            assert!(
                !parts.last().is_some_and(|s| s == "String" || s == "str"),
                "Unrecognized Planus string type: {parts:?}"
            );
            visit_mut::visit_type_path_mut(self, ty);
        }
    }
}
struct CheckStringFields<'a> {
    expected: &'a mut BTreeSet<(String, String)>,
}
impl VisitMut for CheckStringFields<'_> {
    fn visit_item_struct_mut(&mut self, item: &mut syn::ItemStruct) {
        let name = item.ident.to_string();
        for field in &mut item.fields {
            let Some(field_name) = field.ident.as_ref().map(ToString::to_string) else {
                continue;
            };
            if self.expected.remove(&(name.clone(), field_name.clone())) {
                // Each FBS string field must still be present as a wire byte
                // string, including fields nested inside Option/Vec wrappers.
                struct HasWireString(bool);
                impl VisitMut for HasWireString {
                    fn visit_type_path_mut(&mut self, ty: &mut syn::TypePath) {
                        if ty
                            .path
                            .segments
                            .last()
                            .is_some_and(|s| s.ident == "WireString")
                        {
                            self.0 = true;
                        }
                        visit_mut::visit_type_path_mut(self, ty);
                    }
                }
                let mut found = HasWireString(false);
                found.visit_type_mut(&mut field.ty);
                assert!(
                    found.0,
                    "Generated string field missing wire adaptation: {name}.{field_name}"
                );
            }
        }
    }
}
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../../..");
    let schema = root.join("schema/fbs");
    println!("cargo:rerun-if-changed={}", schema.display());
    let declarations = planus_translation::translate_files(&[
        schema.join("querys.fbs"),
        schema.join("results.fbs"),
    ])
    .expect("translate shared FBS");
    println!("cargo:rerun-if-changed=coverage.json");
    let coverage: BTreeMap<String, serde_json::Value> = serde_json::from_str(
        &fs::read_to_string("coverage.json").expect("native coverage manifest"),
    )
    .expect("parse coverage manifest");
    let mut keys = BTreeSet::new();
    let mut string_fields = BTreeSet::new();
    for (path, decl) in &declarations.declarations {
        let name = path.0.last().unwrap();
        keys.insert(name.clone());
        match &decl.kind {
            DeclarationKind::Table(table) => {
                for (field, info) in &table.fields {
                    let key = format!("{name}.{field}");
                    let is_string = matches!(&info.type_.kind, TypeKind::String)
                        || matches!(&info.type_.kind,TypeKind::Vector(t) if matches!(t.kind,TypeKind::String));
                    if is_string {
                        string_fields.insert((name.clone(), field.clone()));
                        let encoding = coverage
                            .get(&key)
                            .and_then(|v| v.get("encoding"))
                            .and_then(|v| v.as_str());
                        assert!(
                            matches!(encoding, Some("mutf8" | "utf8" | "ascii")),
                            "Classify encoding for {key}"
                        );
                    }
                    keys.insert(key);
                }
            }
            DeclarationKind::Struct(s) => {
                for field in s.fields.keys() {
                    keys.insert(format!("{name}.{field}"));
                }
            }
            DeclarationKind::Enum(e) => {
                for variant in e.variants.values() {
                    keys.insert(format!("{name}.{}", variant.name));
                }
            }
            DeclarationKind::Union(u) => {
                for variant in u.variants.keys() {
                    keys.insert(format!("{name}.{variant}"));
                }
            }
            DeclarationKind::RpcService(_) => panic!("New native RPC requires explicit mapping"),
        }
    }
    assert_eq!(keys,coverage.keys().cloned().collect(),"Native schema changed: classify new/removed fields, enums, unions and roots in coverage.json");
    for (key, entry) in &coverage {
        assert!(
            matches!(
                entry.get("exposure").and_then(|v| v.as_str()),
                Some("mapped" | "wire-only" | "deferred")
            ),
            "Classify exposure for {key}"
        );
        assert!(
            entry
                .get("reason")
                .and_then(|v| v.as_str())
                .is_some_and(|s| !s.is_empty()),
            "Explain mapping for {key}"
        );
    }
    let generated =
        planus_codegen::generate_rust(&declarations, false).expect("generate Planus types");
    let mut ast = syn::parse_file(&generated).expect("parse generated Rust");
    let mut strings = WireStrings::default();
    strings.visit_file_mut(&mut ast);
    assert!(
        strings.owned > 0 && strings.borrowed > 0,
        "Planus string representation changed"
    );
    CheckStringFields {
        expected: &mut string_fields,
    }
    .visit_file_mut(&mut ast);
    assert!(
        string_fields.is_empty(),
        "Generated string fields disappeared or changed names: {string_fields:?}"
    );
    let mut remaining = WireStrings::default();
    remaining.visit_file_mut(&mut ast);
    assert_eq!(
        (remaining.owned, remaining.borrowed),
        (0, 0),
        "Unadapted wire string types remain"
    );
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    fs::write(output.join("wire.rs"), prettyplease::unparse(&ast)).unwrap();
}
