use std::{env, path::PathBuf};
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap())
        .join("../../..")
        .canonicalize()
        .unwrap();
    println!("cargo:rerun-if-changed={}", root.join("Core").display());
    println!("cargo:rerun-if-changed=native");
    let dst = cmake::Config::new("native")
        .define("DEXKIT_ROOT", &root)
        .define("DEXKIT_ENABLE_SMALI", "ON")
        .define("DEXKIT_BUILD_SMALI_TESTS", "OFF")
        .define("DEXKIT_ENABLE_INTERNAL_METRICS", "OFF")
        .build();
    println!(
        "cargo:rustc-link-search=native={}",
        dst.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=dexkit_native");
    println!("cargo:rustc-link-lib=static=dexkit_static");
    println!("cargo:rustc-link-lib=z");
    match env::var("CARGO_CFG_TARGET_OS").unwrap().as_str() {
        "macos" => println!("cargo:rustc-link-lib=c++"),
        "linux" => println!("cargo:rustc-link-lib=stdc++"),
        other => panic!("Native MCP currently supports macOS and Linux; got {other}"),
    }
}
