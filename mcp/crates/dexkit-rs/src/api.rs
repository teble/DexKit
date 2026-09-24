//! Version 1 tool DTOs. SDK transport details do not belong in these types.
use crate::{error::Error, query};
use schemars::JsonSchema;
use serde::{Deserialize, Serialize};

pub const API_MAJOR: u32 = 1;
pub const CONTRACT_REVISION: &str = "1.0";
pub const TOOLS: &[&str] = &[
    "capabilities",
    "open",
    "close",
    "find_classes",
    "find_methods",
    "find_fields",
    "describe",
    "relations",
    "page",
    "smali",
    "read_artifact",
];

#[derive(Clone, Debug, Default, Deserialize, Serialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct Empty {}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct Open {
    pub path: String,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Instance {
    pub instance_id: String,
}
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum Kind {
    Class,
    Method,
    Field,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(untagged, rename_all = "camelCase", deny_unknown_fields)]
pub enum Within {
    Entities {
        #[serde(rename = "entityIds")]
        entity_ids: Vec<String>,
    },
    ResultSet {
        #[serde(rename = "resultSetId")]
        result_set_id: String,
    },
}
#[derive(Clone, Debug, Default, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Scope {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub search_packages: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub exclude_packages: Option<Vec<String>>,
    #[serde(default)]
    pub ignore_packages_case: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub in_classes: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub within: Option<Within>,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Query<M> {
    #[serde(default)]
    pub scope: Scope,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub matcher: Option<M>,
}
#[derive(Clone, Copy, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum Select {
    Descriptor,
    Flags,
    Source,
}
pub fn default_page_size() -> u32 {
    100
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Find<M> {
    pub instance_id: String,
    pub query: Query<M>,
    #[serde(default = "default_page_size")]
    pub page_size: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub select: Option<Vec<Select>>,
}
pub type FindClasses = Find<query::ClassMatcher>;
pub type FindMethods = Find<query::MethodMatcher>;
pub type FindFields = Find<query::FieldMatcher>;
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Page {
    pub instance_id: String,
    pub cursor: String,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Target {
    pub instance_id: String,
    pub entity_id: String,
}
#[derive(Clone, Copy, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum Detail {
    Annotations,
    UsingStrings,
    UsingNumbers,
    Opcodes,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Describe {
    pub instance_id: String,
    pub entity_id: String,
    #[serde(default)]
    pub include: Vec<Detail>,
}
#[derive(Clone, Copy, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum Relation {
    Invokes,
    Callers,
    FieldReaders,
    FieldWriters,
    DeclaredMethods,
    FieldReferences,
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Relations {
    pub instance_id: String,
    pub entity_id: String,
    pub relation: Relation,
    #[serde(default = "default_page_size")]
    pub page_size: u32,
}
#[derive(Clone, Copy, Debug, Default, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum DebugMode {
    #[default]
    None,
    Strict,
}
#[derive(Clone, Copy, Debug, Default, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub enum Delivery {
    #[default]
    Inline,
    Artifact,
}
pub fn default_output_bytes() -> u32 {
    1024 * 1024
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Smali {
    pub instance_id: String,
    pub entity_id: String,
    #[serde(default)]
    pub debug: DebugMode,
    #[serde(default = "default_output_bytes")]
    pub max_output_bytes: u32,
    #[serde(default)]
    pub delivery: Delivery,
}
pub fn default_chunk_bytes() -> u32 {
    32768
}
#[derive(Clone, Debug, Deserialize, Serialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct ReadArtifact {
    pub instance_id: String,
    pub artifact_id: String,
    #[serde(default)]
    pub start_byte: u32,
    #[serde(default = "default_chunk_bytes")]
    pub max_bytes: u32,
}

#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(untagged)]
pub enum Reply<T> {
    Success {
        #[schemars(extend("const" = true))]
        ok: bool,
        data: T,
    },
    Failure {
        #[schemars(extend("const" = false))]
        ok: bool,
        error: Error,
    },
}
impl<T> Reply<T> {
    pub fn from_result(result: crate::error::Result<T>) -> Self {
        match result {
            Ok(data) => Self::Success { ok: true, data },
            Err(error) => Self::Failure { ok: false, error },
        }
    }
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Opened {
    pub instance_id: String,
    pub byte_length: String,
    pub dex_count: u32,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Closed {
    pub instance_id: String,
    pub closed: bool,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Source {
    pub dex_index: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source_file: Option<String>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Entity {
    pub entity_id: String,
    pub kind: Kind,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub descriptor: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub modifiers: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub access_flags: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source: Option<Source>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct ResultSetInfo {
    pub id: String,
    pub coverage: String,
    pub total_items: String,
    pub expires_in_seconds: u32,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct PageInfo {
    pub returned: u32,
    pub next_cursor: Option<String>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Found {
    pub instance_id: String,
    pub result_set: ResultSetInfo,
    pub items: Vec<Entity>,
    pub page: PageInfo,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct NumberData {
    pub opcode: u8,
    pub raw_bits: String,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Symbol {
    pub kind: Kind,
    pub descriptor: String,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(tag = "type", content = "value", rename_all = "camelCase")]
pub enum AnnotationLiteral {
    Int8(i8),
    Int16(i16),
    Char(u16),
    Int32(i32),
    Int64(String),
    Float32Bits(String),
    Float64Bits(String),
    String(String),
    Symbol(Symbol),
    Array(Vec<AnnotationLiteral>),
    Annotation(Box<AnnotationData>),
    Null,
    Bool(bool),
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct AnnotationElement {
    pub name: String,
    pub value: AnnotationLiteral,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct AnnotationData {
    pub type_descriptor: String,
    pub visibility: String,
    pub elements: Vec<AnnotationElement>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Described {
    pub entity: Entity,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub annotations: Option<Vec<AnnotationData>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub using_strings: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub using_numbers: Option<Vec<NumberData>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub opcodes: Option<Vec<u8>>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct ArtifactInfo {
    pub id: String,
    pub uri: String,
    pub mime_type: String,
    pub byte_length: String,
    pub sha256: String,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct SmaliOutput {
    pub entity_id: String,
    pub format: String,
    pub scope: String,
    pub content_status: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub artifact: Option<ArtifactInfo>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct ArtifactChunk {
    pub artifact_id: String,
    pub start_byte: u32,
    pub next_byte: Option<u32>,
    pub total_bytes: String,
    pub text: String,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct Capabilities {
    pub api_major: u32,
    pub contract_revision: String,
    pub implementation: String,
    pub tools: Vec<String>,
    pub pagination: String,
    pub native_result_limit: bool,
    pub native_cancellation: bool,
    /// Effective Core worker count per instance for loading, cache initialization and queries.
    pub native_threads: u32,
    pub unicode: String,
    pub result_ttl_seconds: u32,
    /// Maximum input file bytes; zero means no configured ceiling.
    pub max_input_bytes: u64,
    /// Maximum raw DEX bytes or total uncompressed APK DEX bytes; zero disables it.
    pub max_dex_bytes: u64,
    pub max_page_size: u32,
    pub max_instances: u32,
    pub max_result_sets: u32,
    pub max_entities_per_instance: u32,
}
