use crate::{
    api::{self, AnnotationLiteral as V, Kind},
    encoding::WireString,
    error::{Error, Result},
    generated::dexkit::schema as fb,
};

pub(crate) struct Row {
    pub native_id: u64,
    pub entity: api::Entity,
}
fn text(value: Option<WireString>) -> Result<String> {
    value
        .ok_or_else(|| {
            Error::new(
                "native_failure",
                "INVALID_NATIVE_RESULT",
                "Missing required native string",
            )
        })?
        .decode()
        .map_err(Into::into)
}
fn row(
    kind: Kind,
    id: u32,
    dex: u32,
    descriptor: Option<WireString>,
    modifiers: u32,
    flags: u32,
    source: Option<WireString>,
) -> Result<Row> {
    Ok(Row {
        native_id: (u64::from(dex) << 32) | u64::from(id),
        entity: api::Entity {
            entity_id: String::new(),
            kind,
            descriptor: Some(text(descriptor)?),
            modifiers: Some(modifiers),
            access_flags: Some(flags),
            source: Some(api::Source {
                dex_index: dex,
                source_file: source.map(|s| s.decode()).transpose()?,
            }),
        },
    })
}
pub(crate) fn class(value: fb::ClassMeta) -> Result<Row> {
    row(
        Kind::Class,
        value.id,
        value.dex_id,
        value.dex_descriptor,
        value.modifiers,
        value.access_flags,
        value.source_file,
    )
}
pub(crate) fn method(value: fb::MethodMeta) -> Result<Row> {
    row(
        Kind::Method,
        value.id,
        value.dex_id,
        value.dex_descriptor,
        value.modifiers,
        value.access_flags,
        None,
    )
}
pub(crate) fn field(value: fb::FieldMeta) -> Result<Row> {
    row(
        Kind::Field,
        value.id,
        value.dex_id,
        value.dex_descriptor,
        value.modifiers,
        value.access_flags,
        None,
    )
}
pub(crate) fn annotation(value: fb::AnnotationMeta) -> Result<api::AnnotationData> {
    annotation_at(value, 0)
}
fn metadata_depth(depth: usize) -> Result<()> {
    if depth > 32 {
        Err(Error::limit(
            "Annotation metadata nesting exceeds 32 levels",
        ))
    } else {
        Ok(())
    }
}
fn annotation_at(value: fb::AnnotationMeta, depth: usize) -> Result<api::AnnotationData> {
    metadata_depth(depth)?;
    Ok(api::AnnotationData {
        type_descriptor: text(value.type_descriptor)?,
        visibility: match value.visibility {
            fb::AnnotationVisibilityType::Build => "build",
            fb::AnnotationVisibilityType::Runtime => "runtime",
            fb::AnnotationVisibilityType::System => "system",
            fb::AnnotationVisibilityType::None => "none",
        }
        .into(),
        elements: value
            .elements
            .unwrap_or_default()
            .into_iter()
            .map(|e| {
                Ok(api::AnnotationElement {
                    name: text(e.name)?,
                    value: literal(*e.value.ok_or_else(|| Error::native(4))?, depth + 1)?,
                })
            })
            .collect::<Result<_>>()?,
    })
}
fn symbol(kind: Kind, descriptor: Option<WireString>) -> Result<V> {
    Ok(V::Symbol(api::Symbol {
        kind,
        descriptor: text(descriptor)?,
    }))
}
fn literal(value: fb::AnnotationEncodeValueMeta, depth: usize) -> Result<V> {
    metadata_depth(depth)?;
    use fb::AnnotationEncodeValue as N;
    Ok(match value.value.ok_or_else(|| Error::native(4))? {
        N::EncodeValueByte(v) => V::Int8(v.value),
        N::EncodeValueShort(v) => V::Int16(v.value),
        N::EncodeValueChar(v) => {
            let unit = v.value as u16;
            if (0xd800..=0xdfff).contains(&unit) {
                return Err(Error::encoding("Isolated UTF-16 char value"));
            }
            V::Char(unit)
        }
        N::EncodeValueInt(v) => V::Int32(v.value),
        N::EncodeValueLong(v) => V::Int64(v.value.to_string()),
        N::EncodeValueFloat(v) => V::Float32Bits(format!("0x{:08x}", v.value.to_bits())),
        N::EncodeValueDouble(v) => V::Float64Bits(format!("0x{:016x}", v.value.to_bits())),
        N::EncodeValueString(v) => V::String(text(v.value)?),
        N::ClassMeta(v) => symbol(Kind::Class, v.dex_descriptor)?,
        N::MethodMeta(v) => symbol(Kind::Method, v.dex_descriptor)?,
        N::FieldMeta(v) => symbol(Kind::Field, v.dex_descriptor)?,
        N::AnnotationEncodeArray(v) => V::Array(
            v.values
                .unwrap_or_default()
                .into_iter()
                .map(|v| literal(v, depth + 1))
                .collect::<Result<_>>()?,
        ),
        N::AnnotationMeta(v) => V::Annotation(Box::new(annotation_at(*v, depth + 1)?)),
        N::EncodeValueNull(_) => V::Null,
        N::EncodeValueBoolean(v) => V::Bool(v.value),
    })
}
