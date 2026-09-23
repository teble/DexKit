pub mod api;
pub mod encoding;
pub mod error;
mod input;
mod metadata;
mod native;
pub mod query;
pub mod service;

#[allow(clippy::all, dead_code, unused_imports)]
pub(crate) mod generated {
    include!(concat!(env!("OUT_DIR"), "/wire.rs"));
}
