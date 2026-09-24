//! Bounded, transport-independent state. Own this on a single execution thread.
use crate::{
    api::*,
    encoding::WireString,
    error::{Error, Result},
    generated::dexkit::schema as fb,
    input::{Input, InputLimits},
    metadata::{self, Row},
    native::Native,
    query::ToWire,
};
use serde::{de::DeserializeOwned, Serialize};
use serde_json::Value;
use sha2::{Digest, Sha256};
use std::{
    collections::{HashMap, HashSet},
    io::{Read, Seek, SeekFrom, Write},
    path::PathBuf,
    time::{Duration, Instant},
};

const MAX_REPLY: usize = 1024 * 1024;
const MAX_INSTANCES: usize = 4;
const MAX_SETS: usize = 32;
const MAX_ENTITIES: usize = 50000;
const MAX_ENTITY_BYTES: usize = 32 * 1024 * 1024;
const MAX_ARTIFACTS: usize = 16;
const MAX_ARTIFACT_BYTES: usize = 32 * 1024 * 1024;
const TTL: Duration = Duration::from_secs(900);
const MAX_PAGE: u32 = 500;
type NativeScope = (Option<Vec<i64>>, Option<Vec<i64>>);

struct OpenInstance {
    native: Native,
    entities: HashMap<String, Row>,
    identities: HashMap<(Kind, u64), String>,
    entity_bytes: usize,
}
struct StoredSet {
    instance: String,
    kind: Kind,
    ids: Vec<String>,
    page_size: usize,
    select: Option<Vec<Select>>,
    created: Instant,
}
struct Artifact {
    instance: String,
    file: std::fs::File,
    size: usize,
    created: Instant,
}
pub struct AnalysisService {
    roots: Vec<crate::input::Root>,
    input_limits: InputLimits,
    instances: HashMap<String, OpenInstance>,
    results: HashMap<String, StoredSet>,
    artifacts: HashMap<String, Artifact>,
}
fn id(prefix: &str) -> String {
    format!("{prefix}_{}", uuid::Uuid::new_v4().simple())
}
fn io(error: std::io::Error) -> Error {
    Error::new("io", "IO_ERROR", error.to_string())
}
fn missing_instance() -> Error {
    Error::new(
        "invalid_request",
        "INSTANCE_CLOSED",
        "Unknown or closed instance",
    )
}
fn missing_entity() -> Error {
    Error::new(
        "invalid_request",
        "INVALID_ENTITY",
        "Entity does not belong to this instance or kind",
    )
}
fn missing_set() -> Error {
    Error::new(
        "expired",
        "RESULT_SET_EXPIRED",
        "Unknown, closed or expired result set",
    )
}
fn missing_artifact() -> Error {
    Error::new(
        "expired",
        "ARTIFACT_EXPIRED",
        "Unknown, closed or expired artifact",
    )
}

impl AnalysisService {
    pub fn new(roots: Vec<PathBuf>) -> Result<Self> {
        Self::with_input_limits(roots, InputLimits::default())
    }
    pub fn with_input_limits(roots: Vec<PathBuf>, input_limits: InputLimits) -> Result<Self> {
        if roots.is_empty() {
            return Err(Error::invalid("At least one input directory is required"));
        }
        let roots = roots
            .into_iter()
            .map(crate::input::Root::new)
            .collect::<Result<Vec<_>>>()?;
        Ok(Self {
            roots,
            input_limits,
            instances: HashMap::new(),
            results: HashMap::new(),
            artifacts: HashMap::new(),
        })
    }
    pub fn capabilities(&self) -> Capabilities {
        Capabilities {
            api_major: API_MAJOR,
            contract_revision: CONTRACT_REVISION.into(),
            implementation: env!("CARGO_PKG_VERSION").into(),
            tools: TOOLS.iter().map(|s| format!("dexkit_v1_{s}")).collect(),
            pagination: "materializedResultSet".into(),
            native_result_limit: false,
            native_cancellation: false,
            unicode: "normal Unicode via MUTF-8; isolated surrogates rejected".into(),
            result_ttl_seconds: TTL.as_secs() as u32,
            max_input_bytes: self.input_limits.max_input_bytes,
            max_dex_bytes: self.input_limits.max_dex_bytes,
            max_page_size: MAX_PAGE,
            max_instances: MAX_INSTANCES as u32,
            max_result_sets: MAX_SETS as u32,
            max_entities_per_instance: MAX_ENTITIES as u32,
        }
    }
    pub fn call(&mut self, name: &str, arguments: Value) -> Value {
        self.prune();
        let result = self.dispatch(name, arguments);
        bounded_reply(result)
    }
    fn dispatch(&mut self, name: &str, arguments: Value) -> Result<Value> {
        validate_json(&arguments)?;
        macro_rules! run {
            ($ty:ty,$method:ident) => {{
                let request: $ty = parse(arguments)?;
                value(self.$method(request)?)
            }};
        }
        match name.strip_prefix("dexkit_v1_").unwrap_or("") {
            "capabilities" => {
                let _: Empty = parse(arguments)?;
                value(self.capabilities())
            }
            "open" => run!(Open, open),
            "close" => run!(Instance, close),
            "find_classes" => run!(FindClasses, find_classes),
            "find_methods" => run!(FindMethods, find_methods),
            "find_fields" => run!(FindFields, find_fields),
            "page" => run!(Page, page),
            "describe" => run!(Describe, describe),
            "relations" => run!(Relations, relations),
            "smali" => run!(Smali, smali),
            "read_artifact" => run!(ReadArtifact, read_artifact),
            _ => Err(Error::new(
                "invalid_request",
                "UNKNOWN_TOOL",
                "Unknown DexKit tool",
            )),
        }
    }
    fn prune(&mut self) {
        self.results.retain(|_, s| s.created.elapsed() < TTL);
        self.artifacts.retain(|_, s| s.created.elapsed() < TTL);
    }
    fn open(&mut self, request: Open) -> Result<Opened> {
        if self.instances.len() >= MAX_INSTANCES {
            return Err(Error::limit("Close an instance before opening another"));
        }
        let file = crate::input::Root::open(&self.roots, &request.path)?;
        let input = Input::prepare(file, self.input_limits.max_input_bytes)?;
        let opened = Native::open(&input, self.input_limits.max_dex_bytes);
        input.verify_unchanged()?;
        let native = opened?;
        let instance_id = id("i");
        let result = Opened {
            instance_id: instance_id.clone(),
            fingerprint: input.fingerprint,
            byte_length: input.byte_length.to_string(),
            dex_count: native.dex_count,
        };
        self.instances.insert(
            instance_id,
            OpenInstance {
                native,
                entities: HashMap::new(),
                identities: HashMap::new(),
                entity_bytes: 0,
            },
        );
        Ok(result)
    }
    fn close(&mut self, request: Instance) -> Result<Closed> {
        self.instances
            .remove(&request.instance_id)
            .ok_or_else(missing_instance)?;
        self.results
            .retain(|_, s| s.instance != request.instance_id);
        self.artifacts
            .retain(|_, s| s.instance != request.instance_id);
        Ok(Closed {
            instance_id: request.instance_id,
            closed: true,
        })
    }
    fn candidates(&self, instance: &str, kind: Kind, scope: &Scope) -> Result<NativeScope> {
        let state = self.instances.get(instance).ok_or_else(missing_instance)?;
        let resolve = |ids: &[String], kind| -> Result<Vec<i64>> {
            ids.iter()
                .map(|id| {
                    let row = state.entities.get(id).ok_or_else(missing_entity)?;
                    if row.entity.kind != kind {
                        return Err(missing_entity());
                    }
                    Ok(row.native_id as i64)
                })
                .collect()
        };
        let classes = scope
            .in_classes
            .as_ref()
            .map(|ids| resolve(ids, Kind::Class))
            .transpose()?;
        let ids = match &scope.within {
            None => None,
            Some(Within::Entities { entity_ids }) => Some(resolve(entity_ids, kind)?),
            Some(Within::ResultSet { result_set_id }) => {
                let result = self.results.get(result_set_id).ok_or_else(missing_set)?;
                if result.created.elapsed() >= TTL {
                    return Err(missing_set());
                }
                if result.instance != instance || result.kind != kind {
                    return Err(missing_entity());
                }
                Some(resolve(&result.ids, kind)?)
            }
        };
        Ok((classes, ids))
    }
    fn find_classes(&mut self, r: FindClasses) -> Result<Found> {
        let (classes, within) = self.candidates(&r.instance_id, Kind::Class, &r.query.scope)?;
        let candidates = match (classes, within) {
            (Some(a), Some(b)) => {
                let b: HashSet<_> = b.into_iter().collect();
                Some(a.into_iter().filter(|id| b.contains(id)).collect())
            }
            (a, b) => a.or(b),
        };
        let q = fb::FindClass {
            search_packages: packages(r.query.scope.search_packages)?,
            exclude_packages: packages(r.query.scope.exclude_packages)?,
            ignore_packages_case: r.query.scope.ignore_packages_case,
            in_classes: candidates,
            find_first: false,
            matcher: r
                .query
                .matcher
                .map(|m| m.wire().map(Box::new))
                .transpose()?,
        };
        page_size(r.page_size)?;
        let rows = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?
            .native
            .classes(q)?
            .into_iter()
            .map(metadata::class)
            .collect::<Result<_>>()?;
        self.store(&r.instance_id, Kind::Class, rows, r.page_size, r.select)
    }
    fn find_methods(&mut self, r: FindMethods) -> Result<Found> {
        let (classes, methods) = self.candidates(&r.instance_id, Kind::Method, &r.query.scope)?;
        let q = fb::FindMethod {
            search_packages: packages(r.query.scope.search_packages)?,
            exclude_packages: packages(r.query.scope.exclude_packages)?,
            ignore_packages_case: r.query.scope.ignore_packages_case,
            in_classes: classes,
            in_methods: methods,
            find_first: false,
            matcher: r
                .query
                .matcher
                .map(|m| m.wire().map(Box::new))
                .transpose()?,
        };
        page_size(r.page_size)?;
        let rows = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?
            .native
            .methods(q)?
            .into_iter()
            .map(metadata::method)
            .collect::<Result<_>>()?;
        self.store(&r.instance_id, Kind::Method, rows, r.page_size, r.select)
    }
    fn find_fields(&mut self, r: FindFields) -> Result<Found> {
        let (classes, fields) = self.candidates(&r.instance_id, Kind::Field, &r.query.scope)?;
        let q = fb::FindField {
            search_packages: packages(r.query.scope.search_packages)?,
            exclude_packages: packages(r.query.scope.exclude_packages)?,
            ignore_packages_case: r.query.scope.ignore_packages_case,
            in_classes: classes,
            in_fields: fields,
            find_first: false,
            matcher: r
                .query
                .matcher
                .map(|m| m.wire().map(Box::new))
                .transpose()?,
        };
        page_size(r.page_size)?;
        let rows = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?
            .native
            .fields(q)?
            .into_iter()
            .map(metadata::field)
            .collect::<Result<_>>()?;
        self.store(&r.instance_id, Kind::Field, rows, r.page_size, r.select)
    }
    fn store(
        &mut self,
        instance: &str,
        kind: Kind,
        mut rows: Vec<Row>,
        size: u32,
        select: Option<Vec<Select>>,
    ) -> Result<Found> {
        self.prune();
        if self.results.len() >= MAX_SETS {
            return Err(Error::limit(
                "Result set capacity reached; close an instance or wait for expiry",
            ));
        }
        if rows.len() > MAX_ENTITIES {
            return Err(Error::limit(
                "Query has too many entities; narrow the matcher",
            ));
        }
        if self.results.values().map(|s| s.ids.len()).sum::<usize>() + rows.len() > 200000 {
            return Err(Error::limit("Retained result capacity reached"));
        }
        rows.sort_by(|a, b| {
            a.entity
                .descriptor
                .cmp(&b.entity.descriptor)
                .then(a.native_id.cmp(&b.native_id))
        });
        rows.dedup_by_key(|r| r.native_id);
        // Check delivery before publishing handles/result state. Use the
        // worst-case escaped page size and leave room for the envelope/IDs.
        for chunk in rows.chunks(size as usize) {
            let preview: Vec<_> = chunk
                .iter()
                .map(|r| projected(r.entity.clone(), &select))
                .collect();
            if serde_json::to_vec(&preview)
                .map_err(|e| Error::invalid(e.to_string()))?
                .len()
                + 4096
                + size as usize * 64
                > MAX_REPLY
            {
                return Err(Error::limit(
                    "Page exceeds response budget; reduce pageSize or select fewer fields",
                ));
            }
        }
        let state = self
            .instances
            .get_mut(instance)
            .ok_or_else(missing_instance)?;
        let added: Vec<_> = rows
            .iter()
            .filter(|r| !state.identities.contains_key(&(kind, r.native_id)))
            .collect();
        let added_bytes = added.iter().map(|r| row_bytes(&r.entity)).sum::<usize>();
        if state.entities.len() + added.len() > MAX_ENTITIES
            || state.entity_bytes + added_bytes > MAX_ENTITY_BYTES
        {
            return Err(Error::limit(
                "Instance entity cache is full; close and reopen it or narrow queries",
            ));
        }
        state.entity_bytes += added_bytes;
        let mut ids = Vec::with_capacity(rows.len());
        for mut row in rows {
            let identity = (kind, row.native_id);
            let entity_id = state
                .identities
                .entry(identity)
                .or_insert_with(|| id("e"))
                .clone();
            row.entity.entity_id = entity_id.clone();
            state.entities.entry(entity_id.clone()).or_insert(row);
            ids.push(entity_id);
        }
        let result_id = id("r");
        self.results.insert(
            result_id.clone(),
            StoredSet {
                instance: instance.into(),
                kind,
                ids,
                page_size: size as usize,
                select,
                created: Instant::now(),
            },
        );
        self.page_at(instance, &result_id, 0)
    }
    fn page(&self, r: Page) -> Result<Found> {
        let (result, offset) = r
            .cursor
            .rsplit_once(':')
            .ok_or_else(|| Error::invalid("Invalid cursor"))?;
        let offset = offset
            .parse::<usize>()
            .map_err(|_| Error::invalid("Invalid cursor"))?;
        self.page_at(&r.instance_id, result, offset)
    }
    fn page_at(&self, instance: &str, result_id: &str, offset: usize) -> Result<Found> {
        let state = self.instances.get(instance).ok_or_else(missing_instance)?;
        let result = self.results.get(result_id).ok_or_else(missing_set)?;
        if result.created.elapsed() >= TTL {
            return Err(missing_set());
        }
        if result.instance != instance {
            return Err(missing_entity());
        }
        if offset > result.ids.len()
            || (offset != 0
                && (offset == result.ids.len() || !offset.is_multiple_of(result.page_size)))
        {
            return Err(Error::invalid("Cursor offset is invalid"));
        }
        let end = (offset + result.page_size).min(result.ids.len());
        let items = result.ids[offset..end]
            .iter()
            .map(|id| {
                projected(
                    state
                        .entities
                        .get(id)
                        .expect("result entities live until instance closes")
                        .entity
                        .clone(),
                    &result.select,
                )
            })
            .collect::<Vec<_>>();
        Ok(Found {
            instance_id: instance.into(),
            result_set: ResultSetInfo {
                id: result_id.into(),
                coverage: "complete".into(),
                total_items: result.ids.len().to_string(),
                expires_in_seconds: TTL.saturating_sub(result.created.elapsed()).as_secs() as u32,
            },
            page: PageInfo {
                returned: items.len() as u32,
                next_cursor: (end < result.ids.len()).then(|| format!("{result_id}:{end}")),
            },
            items,
        })
    }
    fn describe(&mut self, r: Describe) -> Result<Described> {
        let state = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?;
        let row = state
            .entities
            .get(&r.entity_id)
            .ok_or_else(missing_entity)?;
        let kind = row.entity.kind;
        let native_id = row.native_id;
        let mut out = Described {
            entity: row.entity.clone(),
            annotations: None,
            using_strings: None,
            using_numbers: None,
            opcodes: None,
        };
        for part in r.include {
            match part {
                Detail::Annotations => {
                    let op = match kind {
                        Kind::Class => 4,
                        Kind::Method => 5,
                        Kind::Field => 6,
                    };
                    out.annotations = Some(
                        state
                            .native
                            .annotations(op, native_id)?
                            .into_iter()
                            .map(metadata::annotation)
                            .collect::<Result<_>>()?,
                    );
                }
                _ if kind != Kind::Method => {
                    return Err(Error::invalid("Code details require a method entity"))
                }
                Detail::UsingStrings => out.using_strings = Some(state.native.strings(native_id)?),
                Detail::UsingNumbers => {
                    out.using_numbers = Some(
                        state
                            .native
                            .numbers(native_id)?
                            .into_iter()
                            .map(|n| NumberData {
                                opcode: n.op_code,
                                raw_bits: format!("0x{:016x}", n.raw_bits),
                            })
                            .collect(),
                    )
                }
                Detail::Opcodes => out.opcodes = Some(state.native.call(13, native_id, &[])?),
            }
        }
        Ok(out)
    }
    fn relations(&mut self, r: Relations) -> Result<Found> {
        page_size(r.page_size)?;
        let state = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?;
        let row = state
            .entities
            .get(&r.entity_id)
            .ok_or_else(missing_entity)?;
        let (kind, native_id) = (row.entity.kind, row.native_id);
        let (result_kind, rows) = match (kind, r.relation) {
            (Kind::Class, Relation::DeclaredMethods) => {
                let bytes = state.native.call(14, native_id, &[])?;
                (
                    Kind::Method,
                    Native::decode_methods(&bytes)?
                        .into_iter()
                        .map(metadata::method)
                        .collect::<Result<_>>()?,
                )
            }
            (Kind::Class, Relation::FieldReferences) => (
                Kind::Field,
                state
                    .native
                    .fields(fb::FindField {
                        in_classes: Some(vec![native_id as i64]),
                        ..Default::default()
                    })?
                    .into_iter()
                    .map(metadata::field)
                    .collect::<Result<_>>()?,
            ),
            (Kind::Method, Relation::Invokes | Relation::Callers)
            | (Kind::Field, Relation::FieldReaders | Relation::FieldWriters) => {
                let op = match r.relation {
                    Relation::Invokes => 8,
                    Relation::Callers => 9,
                    Relation::FieldReaders => 10,
                    Relation::FieldWriters => 11,
                    _ => unreachable!(),
                };
                let bytes = state.native.call(op, native_id, &[])?;
                (
                    Kind::Method,
                    Native::decode_methods(&bytes)?
                        .into_iter()
                        .map(metadata::method)
                        .collect::<Result<_>>()?,
                )
            }
            _ => {
                return Err(Error::invalid(
                    "Relation does not apply to this entity kind",
                ))
            }
        };
        self.store(&r.instance_id, result_kind, rows, r.page_size, None)
    }
    fn smali(&mut self, r: Smali) -> Result<SmaliOutput> {
        if r.max_output_bytes == 0 || r.max_output_bytes > 16 * 1024 * 1024 {
            return Err(Error::invalid("maxOutputBytes must be 1..16777216"));
        }
        let state = self
            .instances
            .get_mut(&r.instance_id)
            .ok_or_else(missing_instance)?;
        let row = state
            .entities
            .get(&r.entity_id)
            .ok_or_else(missing_entity)?;
        if row.entity.kind == Kind::Field {
            return Err(Error::invalid("Smali requires a class or method"));
        }
        let method = row.entity.kind == Kind::Method;
        let text = state.native.smali(
            row.native_id,
            method,
            matches!(r.debug, DebugMode::Strict),
            r.max_output_bytes,
        )?;
        let mut out = SmaliOutput {
            entity_id: r.entity_id,
            format: "smali".into(),
            scope: if method { "methodFragment" } else { "class" }.into(),
            content_status: "complete".into(),
            text: None,
            artifact: None,
        };
        match r.delivery {
            Delivery::Inline => {
                if text.len() > 64 * 1024 {
                    return Err(Error::limit(
                        "Inline smali exceeds 64 KiB; request artifact delivery",
                    ));
                }
                out.text = Some(text);
            }
            Delivery::Artifact => {
                self.prune();
                if self.artifacts.len() >= MAX_ARTIFACTS
                    || self.artifacts.values().map(|a| a.size).sum::<usize>() + text.len()
                        > MAX_ARTIFACT_BYTES
                {
                    return Err(Error::limit("Artifact capacity reached"));
                }
                let artifact_id = id("a");
                // Anonymous/unlinked file: even a worker abort cannot leave
                // named smali output behind. Lifetime follows the open FD.
                let mut file = tempfile::tempfile().map_err(io)?;
                file.write_all(text.as_bytes()).map_err(io)?;
                out.artifact = Some(ArtifactInfo {
                    id: artifact_id.clone(),
                    uri: format!("dexkit://artifacts/{artifact_id}"),
                    mime_type: "text/plain".into(),
                    byte_length: text.len().to_string(),
                    sha256: format!("{:x}", Sha256::digest(text.as_bytes())),
                });
                self.artifacts.insert(
                    artifact_id,
                    Artifact {
                        instance: r.instance_id,
                        file,
                        size: text.len(),
                        created: Instant::now(),
                    },
                );
            }
        }
        Ok(out)
    }
    fn read_artifact(&mut self, r: ReadArtifact) -> Result<ArtifactChunk> {
        if r.max_bytes == 0 || r.max_bytes > 65536 {
            return Err(Error::invalid("maxBytes must be 1..65536"));
        }
        if !self.instances.contains_key(&r.instance_id) {
            return Err(missing_instance());
        }
        let a = self
            .artifacts
            .get_mut(&r.artifact_id)
            .ok_or_else(missing_artifact)?;
        if a.created.elapsed() >= TTL {
            return Err(missing_artifact());
        }
        if a.instance != r.instance_id {
            return Err(missing_entity());
        }
        let start = r.start_byte as usize;
        if start > a.size {
            return Err(Error::invalid("startByte is out of range"));
        }
        a.file.seek(SeekFrom::Start(start as u64)).map_err(io)?;
        let mut bytes = vec![0; (r.max_bytes as usize).min(a.size - start)];
        a.file.read_exact(&mut bytes).map_err(io)?;
        let text = match std::str::from_utf8(&bytes) {
            Ok(text) => text.to_owned(),
            Err(e) if e.error_len().is_none() && e.valid_up_to() > 0 => {
                std::str::from_utf8(&bytes[..e.valid_up_to()])
                    .unwrap()
                    .to_owned()
            }
            _ => {
                return Err(Error::invalid(
                    "Range must begin at a UTF-8 boundary and include a complete character",
                ))
            }
        };
        let next = start + text.len();
        Ok(ArtifactChunk {
            artifact_id: r.artifact_id,
            start_byte: r.start_byte,
            next_byte: (next < a.size).then_some(next as u32),
            total_bytes: a.size.to_string(),
            text,
        })
    }
    pub fn resource(&mut self, uri: &str) -> Result<String> {
        if uri.len() > 128 {
            return Err(missing_artifact());
        }
        self.prune();
        let id = uri
            .strip_prefix("dexkit://artifacts/")
            .ok_or_else(missing_artifact)?;
        let a = self.artifacts.get(id).ok_or_else(missing_artifact)?;
        if a.size > 65536 {
            return Err(Error::limit(
                "Use dexkit_v1_read_artifact to read this resource in chunks",
            ));
        }
        let instance_id = a.instance.clone();
        Ok(self
            .read_artifact(ReadArtifact {
                instance_id,
                artifact_id: id.into(),
                start_byte: 0,
                max_bytes: 65536,
            })?
            .text)
    }
}

fn bounded_reply(result: Result<Value>) -> Value {
    fn shape(value: &Value, depth: usize, nodes: &mut usize) -> bool {
        *nodes += 1;
        if depth > 64 || *nodes > 100000 {
            return false;
        }
        match value {
            Value::Object(fields) => fields.values().all(|v| shape(v, depth + 1, nodes)),
            Value::Array(values) => values.iter().all(|v| shape(v, depth + 1, nodes)),
            _ => true,
        }
    }
    let reply =
        serde_json::to_value(Reply::from_result(result)).expect("response DTOs are JSON-safe");
    let message = if !shape(&reply, 0, &mut 0) {
        Some("Response nesting/node budget exceeded; request fewer details")
    } else if serde_json::to_vec(&reply).expect("JSON value").len() > MAX_REPLY {
        Some("Response exceeds 1 MiB; reduce pageSize/select/include or use artifact delivery")
    } else {
        None
    };
    match message {
        Some(message) => serde_json::to_value(Reply::<Value>::Failure {
            ok: false,
            error: Error::limit(message),
        })
        .unwrap(),
        None => reply,
    }
}
fn value<T: Serialize>(value: T) -> Result<Value> {
    serde_json::to_value(value).map_err(|e| Error::new("internal", "SERIALIZATION", e.to_string()))
}
fn parse<T: DeserializeOwned>(value: Value) -> Result<T> {
    serde_json::from_value(value).map_err(|e| Error::invalid(e.to_string()))
}
fn page_size(size: u32) -> Result<()> {
    if size == 0 || size > MAX_PAGE {
        Err(Error::invalid("pageSize must be 1..500"))
    } else {
        Ok(())
    }
}
fn row_bytes(e: &Entity) -> usize {
    256 + e.descriptor.as_ref().map_or(0, String::len)
        + e.source
            .as_ref()
            .and_then(|s| s.source_file.as_ref())
            .map_or(0, String::len)
}
fn projected(mut entity: Entity, select: &Option<Vec<Select>>) -> Entity {
    if let Some(select) = select {
        if !select.iter().any(|x| matches!(x, Select::Descriptor)) {
            entity.descriptor = None;
        }
        if !select.iter().any(|x| matches!(x, Select::Flags)) {
            entity.modifiers = None;
            entity.access_flags = None;
        }
        if !select.iter().any(|x| matches!(x, Select::Source)) {
            entity.source = None;
        }
    }
    entity
}
fn packages(value: Option<Vec<String>>) -> Result<Option<Vec<WireString>>> {
    value
        .map(|v| {
            v.into_iter()
                .map(|s| {
                    if s.is_empty() || s.contains('\0') || s.len() > 65536 {
                        return Err(Error::invalid(
                            "Package names must be nonempty, NUL-free and at most 64 KiB",
                        ));
                    }
                    Ok(WireString::mutf8(s))
                })
                .collect()
        })
        .transpose()
}

pub fn validate_json(value: &Value) -> Result<()> {
    fn walk(value: &Value, depth: usize, nodes: &mut usize, allow_null: bool) -> Result<()> {
        *nodes += 1;
        if depth > 32 || *nodes > 10000 {
            return Err(Error::limit("Request nesting/node budget exceeded"));
        }
        match value {
            Value::Null if !allow_null => {
                return Err(Error::invalid(
                    "null is only allowed in parameter wildcard slots; omit unused fields",
                ))
            }
            Value::Object(fields) => {
                for (name, value) in fields {
                    if name == "parameters" && value.is_array() {
                        for slot in value.as_array().unwrap() {
                            walk(slot, depth + 1, nodes, true)?;
                        }
                    } else {
                        walk(value, depth + 1, nodes, false)?;
                    }
                }
            }
            Value::Array(items) => {
                for item in items {
                    walk(item, depth + 1, nodes, false)?;
                }
            }
            _ => (),
        }
        Ok(())
    }
    if !value.is_object() {
        return Err(Error::invalid("Tool arguments must be an object"));
    }
    walk(value, 0, &mut 0, false)?;
    if serde_json::to_vec(value)
        .map_err(|e| Error::invalid(e.to_string()))?
        .len()
        > 1024 * 1024
    {
        return Err(Error::limit("Request exceeds 1 MiB"));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deep_reply_is_an_error_before_worker_serialization() {
        let mut value = serde_json::json!(0);
        for _ in 0..140 {
            value = serde_json::json!({"nested":value});
        }
        let reply = bounded_reply(Ok(value));
        assert_eq!(reply["error"]["code"], "LIMIT_EXCEEDED");
        let bytes = serde_json::to_vec(&reply).unwrap();
        assert!(serde_json::from_slice::<Value>(&bytes).is_ok());
        assert_eq!(
            bounded_reply(Ok(serde_json::json!({"small":true})))["ok"],
            true
        );
    }

    #[test]
    fn every_page_is_checked_before_publishing_state() {
        let root = tempfile::tempdir().unwrap();
        let mut service = AnalysisService::new(vec![root.path().into()]).unwrap();
        // The first page is tiny; the second exceeds the response budget.
        let rows = (0..4)
            .map(|n| Row {
                native_id: n,
                entity: Entity {
                    entity_id: String::new(),
                    kind: Kind::Class,
                    descriptor: Some(format!("L{n};")),
                    modifiers: None,
                    access_flags: None,
                    source: Some(Source {
                        dex_index: 0,
                        source_file: Some(if n < 2 {
                            "tiny".into()
                        } else {
                            "x".repeat(600_000)
                        }),
                    }),
                },
            })
            .collect();
        let error = service
            .store("unused", Kind::Class, rows, 2, None)
            .unwrap_err();
        assert_eq!(error.code, "LIMIT_EXCEEDED");
        assert!(error.message.contains("Page"));
        assert!(service.results.is_empty());
        assert!(service.instances.is_empty());
    }

    #[test]
    fn expiry_drops_artifact_files_and_result_handles() {
        let root = tempfile::tempdir().unwrap();
        let mut service = AnalysisService::new(vec![root.path().into()]).unwrap();
        let file = tempfile::tempfile().unwrap();
        use std::os::unix::fs::MetadataExt;
        assert_eq!(file.metadata().unwrap().nlink(), 0);
        let expired = Instant::now() - TTL - Duration::from_secs(1);
        service.artifacts.insert(
            "a_test".into(),
            Artifact {
                instance: "test".into(),
                file,
                size: 0,
                created: expired,
            },
        );
        service.results.insert(
            "r_test".into(),
            StoredSet {
                instance: "test".into(),
                kind: Kind::Class,
                ids: vec![],
                page_size: 1,
                select: None,
                created: expired,
            },
        );
        service.prune();
        assert!(service.artifacts.is_empty());
        assert!(service.results.is_empty());
        assert_eq!(
            service
                .resource("dexkit://artifacts/a_test")
                .unwrap_err()
                .code,
            "ARTIFACT_EXPIRED"
        );
    }
}
