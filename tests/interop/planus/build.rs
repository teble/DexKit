use std::{env, path::PathBuf};
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap())
        .join("../../..")
        .canonicalize()
        .unwrap();
    println!("cargo:rerun-if-changed=native");
    println!(
        "cargo:rerun-if-changed={}",
        root.join("Core/dexkit/include/schema").display()
    );
    let dst = cmake::Config::new("native")
        .define("DEXKIT_ROOT", root)
        .build();
    println!(
        "cargo:rustc-link-search=native={}",
        dst.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=dexkit_reference");
}
