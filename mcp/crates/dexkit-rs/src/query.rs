//! Public JSON query contract. This is deliberately distinct from generated FBS.
use crate::{
    encoding::WireString,
    error::{Error, Result},
    generated::dexkit::schema as fb,
};
use schemars::JsonSchema;
use serde::{Deserialize, Serialize};

macro_rules! matcher {
    ($name:ident { $($field:ident : $ty:ty),* $(,)? }) => {
        #[derive(Clone, Debug, Default, Serialize, Deserialize, JsonSchema)]
        #[serde(rename_all = "camelCase", deny_unknown_fields)]
        pub struct $name { $(#[serde(default, skip_serializing_if = "Option::is_none")] pub $field: Option<$ty>,)* }
    }
}
macro_rules! enum_map {
    ($name:ident => $native:ident { $($variant:ident),+ $(,)? }) => {
        #[derive(Clone, Copy, Debug, Serialize, Deserialize, JsonSchema)]
        #[serde(rename_all = "camelCase")]
        pub enum $name { $($variant),+ }
        impl From<$name> for fb::$native {
            fn from(value: $name) -> Self { match value { $($name::$variant => Self::$variant),+ } }
        }
    }
}
enum_map!(StringMatch => StringMatchType { Contains, StartWith, EndWith, Equal });
enum_map!(CollectionMatch => MatchType { Contains, Equal });
enum_map!(OpcodeMatch => OpCodeMatchType { Contains, StartWith, EndWith, Equal });
enum_map!(UsingType => UsingType { Any, Get, Put });
enum_map!(Retention => RetentionPolicyType { Source, Class, Runtime });
enum_map!(TargetElement => TargetElementType { Type, Field, Method, Parameter, Constructor,
    LocalVariable, AnnotationType, Package, TypeParameter, TypeUse });

#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Text {
    pub value: String,
    #[serde(rename = "match")]
    pub match_type: StringMatch,
    #[serde(default)]
    pub ignore_case: bool,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct Range {
    pub min: u32,
    pub max: u32,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(deny_unknown_fields)]
pub struct Flags {
    pub flags: u32,
    #[serde(rename = "match")]
    pub match_type: CollectionMatch,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(
    tag = "type",
    content = "value",
    rename_all = "camelCase",
    deny_unknown_fields
)]
pub enum Number {
    Int8(i8),
    Int16(i16),
    Int32(i32),
    Int64(String),
    Float32(f32),
    Float64(f64),
}

matcher!(ClassMatcher {
    smali_source: Box<Text>, class_name: Box<Text>, modifiers: Box<Flags>, access_flags: Box<Flags>,
    super_class: Box<ClassMatcher>, interfaces: Box<Interfaces>, annotations: Box<Annotations>,
    fields: Box<Fields>, methods: Box<Methods>, using_strings: Vec<Text>,
    all_of: Vec<ClassMatcher>, any_of: Vec<ClassMatcher>, none_of: Vec<ClassMatcher>,
});
matcher!(MethodMatcher {
    method_name: Box<Text>, modifiers: Box<Flags>, access_flags: Box<Flags>, declaring_class: Box<ClassMatcher>,
    return_type: Box<ClassMatcher>, parameters: Box<Parameters>, annotations: Box<Annotations>,
    op_codes: Box<Opcodes>, using_strings: Vec<Text>, using_fields: Vec<UsingField>, using_numbers: Vec<Number>,
    invoking_methods: Box<Methods>, method_callers: Box<Methods>, proto_shorty: String,
    all_of: Vec<MethodMatcher>, any_of: Vec<MethodMatcher>, none_of: Vec<MethodMatcher>,
});
matcher!(FieldMatcher {
    field_name: Box<Text>, modifiers: Box<Flags>, access_flags: Box<Flags>, declaring_class: Box<ClassMatcher>,
    type_class: Box<ClassMatcher>, annotations: Box<Annotations>, get_methods: Box<Methods>, put_methods: Box<Methods>,
    all_of: Vec<FieldMatcher>, any_of: Vec<FieldMatcher>, none_of: Vec<FieldMatcher>,
});
matcher!(Parameters { parameters: Vec<Option<Parameter>>, parameter_count: Box<Range> });
matcher!(Parameter { annotations: Box<Annotations>, parameter_type: Box<ClassMatcher> });
matcher!(Opcodes { op_codes: Vec<u8>, match_type: OpcodeMatch, op_code_count: Box<Range> });
matcher!(Methods { methods: Vec<MethodMatcher>, match_type: CollectionMatch, method_count: Box<Range> });
matcher!(Fields { fields: Vec<FieldMatcher>, match_type: CollectionMatch, field_count: Box<Range> });
matcher!(Interfaces { interfaces: Vec<ClassMatcher>, match_type: CollectionMatch, interface_count: Box<Range> });
matcher!(UsingField { field: Box<FieldMatcher>, using_type: UsingType });
matcher!(Annotations { annotations: Vec<Annotation>, match_type: CollectionMatch, annotation_count: Box<Range> });
matcher!(Annotation {
    annotation_type: Box<ClassMatcher>, target_element_types: Box<TargetElements>, policy: Retention,
    elements: Box<AnnotationElements>, using_strings: Vec<Text>,
});
matcher!(TargetElements { types: Vec<TargetElement>, match_type: CollectionMatch });
matcher!(AnnotationElements { elements: Vec<AnnotationElement>, match_type: CollectionMatch, element_count: Box<Range> });
matcher!(AnnotationElement { name: Box<Text>, value: AnnotationValue });
matcher!(AnnotationArray { values: Vec<AnnotationValue>, match_type: CollectionMatch, value_count: Box<Range> });
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(
    tag = "type",
    content = "value",
    rename_all = "camelCase",
    deny_unknown_fields
)]
pub enum AnnotationValue {
    Int8(i8),
    Int16(i16),
    Char(u16),
    Int32(i32),
    Int64(String),
    Float32(f32),
    Float64(f64),
    String(Text),
    Class(Box<ClassMatcher>),
    Method(Box<MethodMatcher>),
    Enum(Box<FieldMatcher>),
    Array(AnnotationArray),
    Annotation(Box<Annotation>),
    Null,
    Bool(bool),
}

pub(crate) trait ToWire {
    type Output;
    fn wire(self) -> Result<Self::Output>;
}
fn boxed<T: ToWire>(value: Option<Box<T>>) -> Result<Option<Box<T::Output>>> {
    value.map(|v| v.wire().map(Box::new)).transpose()
}
fn list<T: ToWire>(value: Option<Vec<T>>) -> Result<Option<Vec<T::Output>>> {
    value
        .map(|v| v.into_iter().map(ToWire::wire).collect())
        .transpose()
}
fn logical<T: ToWire>(value: Option<Vec<T>>) -> Result<Option<Vec<T::Output>>> {
    if value.as_ref().is_some_and(Vec::is_empty) {
        return Err(Error::invalid("Boolean matcher arrays must be nonempty"));
    }
    list(value)
}
fn collection(value: Option<CollectionMatch>) -> fb::MatchType {
    value.map(Into::into).unwrap_or(fb::MatchType::Contains)
}
fn int64(value: &str) -> Result<i64> {
    let digits = value.strip_prefix('-').unwrap_or(value);
    if digits.is_empty() || !digits.bytes().all(|b| b.is_ascii_digit()) {
        return Err(Error::invalid("int64 value must be a decimal string"));
    }
    value
        .parse()
        .map_err(|_| Error::invalid("int64 is out of range"))
}
fn finite32(value: f32) -> Result<f32> {
    if value.is_finite() {
        Ok(value)
    } else {
        Err(Error::invalid("float32 must be finite"))
    }
}
fn finite64(value: f64) -> Result<f64> {
    if value.is_finite() {
        Ok(value)
    } else {
        Err(Error::invalid("float64 must be finite"))
    }
}
impl ToWire for Text {
    type Output = fb::StringMatcher;
    fn wire(self) -> Result<Self::Output> {
        if self.value.len() > 64 * 1024 {
            return Err(Error::limit("String matcher exceeds 64 KiB UTF-8"));
        }
        if self.value.is_empty() && !matches!(self.match_type, StringMatch::Equal) {
            return Err(Error::invalid(
                "An empty string matcher must use match: equal",
            ));
        }
        let value = WireString::mutf8(self.value);
        if value.as_bytes().len() > 128 * 1024 {
            return Err(Error::limit("String matcher exceeds 128 KiB MUTF-8"));
        }
        Ok(fb::StringMatcher {
            value: Some(value),
            match_type: self.match_type.into(),
            ignore_case: self.ignore_case,
        })
    }
}
impl ToWire for Range {
    type Output = fb::IntRange;
    fn wire(self) -> Result<Self::Output> {
        if self.min > self.max || self.max > i32::MAX as u32 {
            return Err(Error::invalid(
                "Count range requires 0 <= min <= max <= 2147483647",
            ));
        }
        Ok(fb::IntRange {
            min: self.min as i32,
            max: self.max as i32,
        })
    }
}
impl ToWire for Flags {
    type Output = fb::AccessFlagsMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::AccessFlagsMatcher {
            flags: self.flags,
            match_type: self.match_type.into(),
        })
    }
}
impl ToWire for Number {
    type Output = fb::Number;
    fn wire(self) -> Result<Self::Output> {
        use fb::Number as N;
        Ok(match self {
            Self::Int8(value) => N::EncodeValueByte(Box::new(fb::EncodeValueByte { value })),
            Self::Int16(value) => N::EncodeValueShort(Box::new(fb::EncodeValueShort { value })),
            Self::Int32(value) => N::EncodeValueInt(Box::new(fb::EncodeValueInt { value })),
            Self::Int64(value) => N::EncodeValueLong(Box::new(fb::EncodeValueLong {
                value: int64(&value)?,
            })),
            Self::Float32(value) => N::EncodeValueFloat(Box::new(fb::EncodeValueFloat {
                value: finite32(value)?,
            })),
            Self::Float64(value) => N::EncodeValueDouble(Box::new(fb::EncodeValueDouble {
                value: finite64(value)?,
            })),
        })
    }
}
impl ToWire for ClassMatcher {
    type Output = fb::ClassMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::ClassMatcher {
            smali_source: boxed(self.smali_source)?,
            class_name: boxed(self.class_name)?,
            modifiers: boxed(self.modifiers)?,
            access_flags: boxed(self.access_flags)?,
            super_class: boxed(self.super_class)?,
            interfaces: boxed(self.interfaces)?,
            annotations: boxed(self.annotations)?,
            fields: boxed(self.fields)?,
            methods: boxed(self.methods)?,
            using_strings: list(self.using_strings)?,
            all_of: logical(self.all_of)?,
            any_of: logical(self.any_of)?,
            none_of: logical(self.none_of)?,
        })
    }
}
impl ToWire for MethodMatcher {
    type Output = fb::MethodMatcher;
    fn wire(self) -> Result<Self::Output> {
        if let Some(s) = &self.proto_shorty {
            let mut chars = s.chars();
            if !chars.next().is_some_and(|c| "VZBSCIJFDL".contains(c))
                || !chars.all(|c| "ZBSCIJFDL".contains(c))
            {
                return Err(Error::invalid("protoShorty must be a DEX short descriptor"));
            }
        }
        Ok(fb::MethodMatcher {
            method_name: boxed(self.method_name)?,
            modifiers: boxed(self.modifiers)?,
            access_flags: boxed(self.access_flags)?,
            declaring_class: boxed(self.declaring_class)?,
            return_type: boxed(self.return_type)?,
            parameters: boxed(self.parameters)?,
            annotations: boxed(self.annotations)?,
            op_codes: boxed(self.op_codes)?,
            using_strings: list(self.using_strings)?,
            using_fields: list(self.using_fields)?,
            using_numbers: list(self.using_numbers)?,
            invoking_methods: boxed(self.invoking_methods)?,
            method_callers: boxed(self.method_callers)?,
            proto_shorty: self.proto_shorty.map(WireString::utf8),
            all_of: logical(self.all_of)?,
            any_of: logical(self.any_of)?,
            none_of: logical(self.none_of)?,
        })
    }
}
impl ToWire for FieldMatcher {
    type Output = fb::FieldMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::FieldMatcher {
            field_name: boxed(self.field_name)?,
            modifiers: boxed(self.modifiers)?,
            access_flags: boxed(self.access_flags)?,
            declaring_class: boxed(self.declaring_class)?,
            type_class: boxed(self.type_class)?,
            annotations: boxed(self.annotations)?,
            get_methods: boxed(self.get_methods)?,
            put_methods: boxed(self.put_methods)?,
            all_of: logical(self.all_of)?,
            any_of: logical(self.any_of)?,
            none_of: logical(self.none_of)?,
        })
    }
}
macro_rules! collection_wire {
    ($ty:ident, $native:ident, $items:ident, $count:ident) => {
        impl ToWire for $ty {
            type Output = fb::$native;
            fn wire(self) -> Result<Self::Output> {
                Ok(fb::$native {
                    $items: list(self.$items)?,
                    $count: boxed(self.$count)?,
                    match_type: collection(self.match_type),
                })
            }
        }
    };
}
collection_wire!(Methods, MethodsMatcher, methods, method_count);
collection_wire!(Fields, FieldsMatcher, fields, field_count);
collection_wire!(Interfaces, InterfacesMatcher, interfaces, interface_count);
collection_wire!(
    Annotations,
    AnnotationsMatcher,
    annotations,
    annotation_count
);
collection_wire!(
    AnnotationElements,
    AnnotationElementsMatcher,
    elements,
    element_count
);
collection_wire!(
    AnnotationArray,
    AnnotationEncodeArrayMatcher,
    values,
    value_count
);
impl ToWire for Parameter {
    type Output = fb::ParameterMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::ParameterMatcher {
            annotations: boxed(self.annotations)?,
            parameter_type: boxed(self.parameter_type)?,
        })
    }
}
impl ToWire for Parameters {
    type Output = fb::ParametersMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::ParametersMatcher {
            parameters: list(
                self.parameters
                    .map(|p| p.into_iter().map(Option::unwrap_or_default).collect()),
            )?,
            parameter_count: boxed(self.parameter_count)?,
        })
    }
}
impl ToWire for Opcodes {
    type Output = fb::OpCodesMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::OpCodesMatcher {
            op_codes: self
                .op_codes
                .map(|v| v.into_iter().map(i16::from).collect()),
            match_type: self
                .match_type
                .map(Into::into)
                .unwrap_or(fb::OpCodeMatchType::Contains),
            op_code_count: boxed(self.op_code_count)?,
        })
    }
}
impl ToWire for UsingField {
    type Output = fb::UsingFieldMatcher;
    fn wire(self) -> Result<Self::Output> {
        if self.field.is_none() {
            return Err(Error::invalid("usingFields entries require field"));
        }
        Ok(fb::UsingFieldMatcher {
            field: boxed(self.field)?,
            using_type: self
                .using_type
                .map(Into::into)
                .unwrap_or(fb::UsingType::Any),
        })
    }
}
impl ToWire for TargetElements {
    type Output = fb::TargetElementTypesMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::TargetElementTypesMatcher {
            types: self.types.map(|v| v.into_iter().map(Into::into).collect()),
            match_type: collection(self.match_type),
        })
    }
}
impl ToWire for Annotation {
    type Output = fb::AnnotationMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::AnnotationMatcher {
            type_: boxed(self.annotation_type)?,
            target_element_types: boxed(self.target_element_types)?,
            policy: self
                .policy
                .map(Into::into)
                .unwrap_or(fb::RetentionPolicyType::Any),
            elements: boxed(self.elements)?,
            using_strings: list(self.using_strings)?,
        })
    }
}
impl ToWire for AnnotationElement {
    type Output = fb::AnnotationElementMatcher;
    fn wire(self) -> Result<Self::Output> {
        Ok(fb::AnnotationElementMatcher {
            name: boxed(self.name)?,
            value: self.value.map(ToWire::wire).transpose()?,
        })
    }
}
impl ToWire for AnnotationValue {
    type Output = fb::AnnotationEncodeValueMatcher;
    fn wire(self) -> Result<Self::Output> {
        use fb::AnnotationEncodeValueMatcher as V;
        Ok(match self {
            Self::Int8(value) => V::EncodeValueByte(Box::new(fb::EncodeValueByte { value })),
            Self::Int16(value) => V::EncodeValueShort(Box::new(fb::EncodeValueShort { value })),
            Self::Char(value) => {
                if (0xd800..=0xdfff).contains(&value) {
                    return Err(Error::encoding("Surrogate char values are unsupported"));
                }
                // Existing FBS uses signed short; preserve the UTF-16 bits.
                V::EncodeValueChar(Box::new(fb::EncodeValueChar {
                    value: value as i16,
                }))
            }
            Self::Int32(value) => V::EncodeValueInt(Box::new(fb::EncodeValueInt { value })),
            Self::Int64(value) => V::EncodeValueLong(Box::new(fb::EncodeValueLong {
                value: int64(&value)?,
            })),
            Self::Float32(value) => V::EncodeValueFloat(Box::new(fb::EncodeValueFloat {
                value: finite32(value)?,
            })),
            Self::Float64(value) => V::EncodeValueDouble(Box::new(fb::EncodeValueDouble {
                value: finite64(value)?,
            })),
            Self::String(value) => V::StringMatcher(Box::new(value.wire()?)),
            Self::Class(value) => V::ClassMatcher(Box::new(value.wire()?)),
            Self::Method(value) => V::MethodMatcher(Box::new(value.wire()?)),
            Self::Enum(value) => V::FieldMatcher(Box::new(value.wire()?)),
            Self::Array(value) => V::AnnotationEncodeArrayMatcher(Box::new(value.wire()?)),
            Self::Annotation(value) => V::AnnotationMatcher(Box::new(value.wire()?)),
            Self::Null => V::EncodeValueNull(Box::default()),
            Self::Bool(value) => V::EncodeValueBoolean(Box::new(fb::EncodeValueBoolean { value })),
        })
    }
}
