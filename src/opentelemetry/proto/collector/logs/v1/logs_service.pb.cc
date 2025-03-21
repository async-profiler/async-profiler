// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: opentelemetry/proto/collector/logs/v1/logs_service.proto

#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

namespace opentelemetry {
namespace proto {
namespace collector {
namespace logs {
namespace v1 {
PROTOBUF_CONSTEXPR ExportLogsServiceRequest::ExportLogsServiceRequest(
    ::_pbi::ConstantInitialized): _impl_{
    /*decltype(_impl_.resource_logs_)*/{}
  , /*decltype(_impl_._cached_size_)*/{}} {}
struct ExportLogsServiceRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ExportLogsServiceRequestDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~ExportLogsServiceRequestDefaultTypeInternal() {}
  union {
    ExportLogsServiceRequest _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ExportLogsServiceRequestDefaultTypeInternal _ExportLogsServiceRequest_default_instance_;
PROTOBUF_CONSTEXPR ExportLogsServiceResponse::ExportLogsServiceResponse(
    ::_pbi::ConstantInitialized): _impl_{
    /*decltype(_impl_.partial_success_)*/nullptr
  , /*decltype(_impl_._cached_size_)*/{}} {}
struct ExportLogsServiceResponseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ExportLogsServiceResponseDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~ExportLogsServiceResponseDefaultTypeInternal() {}
  union {
    ExportLogsServiceResponse _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ExportLogsServiceResponseDefaultTypeInternal _ExportLogsServiceResponse_default_instance_;
PROTOBUF_CONSTEXPR ExportLogsPartialSuccess::ExportLogsPartialSuccess(
    ::_pbi::ConstantInitialized): _impl_{
    /*decltype(_impl_.error_message_)*/{&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}}
  , /*decltype(_impl_.rejected_log_records_)*/int64_t{0}
  , /*decltype(_impl_._cached_size_)*/{}} {}
struct ExportLogsPartialSuccessDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ExportLogsPartialSuccessDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~ExportLogsPartialSuccessDefaultTypeInternal() {}
  union {
    ExportLogsPartialSuccess _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ExportLogsPartialSuccessDefaultTypeInternal _ExportLogsPartialSuccess_default_instance_;
}  // namespace v1
}  // namespace logs
}  // namespace collector
}  // namespace proto
}  // namespace opentelemetry
static ::_pb::Metadata file_level_metadata_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto[3];
static constexpr ::_pb::EnumDescriptor const** file_level_enum_descriptors_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto = nullptr;
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto = nullptr;

const uint32_t TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest, _impl_.resource_logs_),
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse, _impl_.partial_success_),
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess, _impl_.rejected_log_records_),
  PROTOBUF_FIELD_OFFSET(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess, _impl_.error_message_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, -1, -1, sizeof(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest)},
  { 7, -1, -1, sizeof(::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse)},
  { 14, -1, -1, sizeof(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::opentelemetry::proto::collector::logs::v1::_ExportLogsServiceRequest_default_instance_._instance,
  &::opentelemetry::proto::collector::logs::v1::_ExportLogsServiceResponse_default_instance_._instance,
  &::opentelemetry::proto::collector::logs::v1::_ExportLogsPartialSuccess_default_instance_._instance,
};

const char descriptor_table_protodef_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n8opentelemetry/proto/collector/logs/v1/"
  "logs_service.proto\022%opentelemetry.proto."
  "collector.logs.v1\032&opentelemetry/proto/l"
  "ogs/v1/logs.proto\"\\\n\030ExportLogsServiceRe"
  "quest\022@\n\rresource_logs\030\001 \003(\0132).opentelem"
  "etry.proto.logs.v1.ResourceLogs\"u\n\031Expor"
  "tLogsServiceResponse\022X\n\017partial_success\030"
  "\001 \001(\0132\?.opentelemetry.proto.collector.lo"
  "gs.v1.ExportLogsPartialSuccess\"O\n\030Export"
  "LogsPartialSuccess\022\034\n\024rejected_log_recor"
  "ds\030\001 \001(\003\022\025\n\rerror_message\030\002 \001(\t2\235\001\n\013Logs"
  "Service\022\215\001\n\006Export\022\?.opentelemetry.proto"
  ".collector.logs.v1.ExportLogsServiceRequ"
  "est\032@.opentelemetry.proto.collector.logs"
  ".v1.ExportLogsServiceResponse\"\000B\230\001\n(io.o"
  "pentelemetry.proto.collector.logs.v1B\020Lo"
  "gsServiceProtoP\001Z0go.opentelemetry.io/pr"
  "oto/otlp/collector/logs/v1\252\002%OpenTelemet"
  "ry.Proto.Collector.Logs.V1b\006proto3"
  ;
static const ::_pbi::DescriptorTable* const descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_deps[1] = {
  &::descriptor_table_opentelemetry_2fproto_2flogs_2fv1_2flogs_2eproto,
};
static ::_pbi::once_flag descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto = {
    false, false, 754, descriptor_table_protodef_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto,
    "opentelemetry/proto/collector/logs/v1/logs_service.proto",
    &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_once, descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_deps, 1, 3,
    schemas, file_default_instances, TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto::offsets,
    file_level_metadata_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto, file_level_enum_descriptors_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto,
    file_level_service_descriptors_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_getter() {
  return &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto;
}

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto(&descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto);
namespace opentelemetry {
namespace proto {
namespace collector {
namespace logs {
namespace v1 {

// ===================================================================

class ExportLogsServiceRequest::_Internal {
 public:
};

void ExportLogsServiceRequest::clear_resource_logs() {
  _impl_.resource_logs_.Clear();
}
ExportLogsServiceRequest::ExportLogsServiceRequest(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor(arena, is_message_owned);
  // @@protoc_insertion_point(arena_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
}
ExportLogsServiceRequest::ExportLogsServiceRequest(const ExportLogsServiceRequest& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  ExportLogsServiceRequest* const _this = this; (void)_this;
  new (&_impl_) Impl_{
      decltype(_impl_.resource_logs_){from._impl_.resource_logs_}
    , /*decltype(_impl_._cached_size_)*/{}};

  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  // @@protoc_insertion_point(copy_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
}

inline void ExportLogsServiceRequest::SharedCtor(
    ::_pb::Arena* arena, bool is_message_owned) {
  (void)arena;
  (void)is_message_owned;
  new (&_impl_) Impl_{
      decltype(_impl_.resource_logs_){arena}
    , /*decltype(_impl_._cached_size_)*/{}
  };
}

ExportLogsServiceRequest::~ExportLogsServiceRequest() {
  // @@protoc_insertion_point(destructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void ExportLogsServiceRequest::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  _impl_.resource_logs_.~RepeatedPtrField();
}

void ExportLogsServiceRequest::SetCachedSize(int size) const {
  _impl_._cached_size_.Set(size);
}

void ExportLogsServiceRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.resource_logs_.Clear();
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* ExportLogsServiceRequest::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      // repeated .opentelemetry.proto.logs.v1.ResourceLogs resource_logs = 1;
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          ptr -= 1;
          do {
            ptr += 1;
            ptr = ctx->ParseMessage(_internal_add_resource_logs(), ptr);
            CHK_(ptr);
            if (!ctx->DataAvailable(ptr)) break;
          } while (::PROTOBUF_NAMESPACE_ID::internal::ExpectTag<10>(ptr));
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }  // switch
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }  // while
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* ExportLogsServiceRequest::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  // repeated .opentelemetry.proto.logs.v1.ResourceLogs resource_logs = 1;
  for (unsigned i = 0,
      n = static_cast<unsigned>(this->_internal_resource_logs_size()); i < n; i++) {
    const auto& repfield = this->_internal_resource_logs(i);
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::
        InternalWriteMessage(1, repfield, repfield.GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  return target;
}

size_t ExportLogsServiceRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated .opentelemetry.proto.logs.v1.ResourceLogs resource_logs = 1;
  total_size += 1UL * this->_internal_resource_logs_size();
  for (const auto& msg : this->_impl_.resource_logs_) {
    total_size +=
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::MessageSize(msg);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData ExportLogsServiceRequest::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSourceCheck,
    ExportLogsServiceRequest::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*ExportLogsServiceRequest::GetClassData() const { return &_class_data_; }


void ExportLogsServiceRequest::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg) {
  auto* const _this = static_cast<ExportLogsServiceRequest*>(&to_msg);
  auto& from = static_cast<const ExportLogsServiceRequest&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  GOOGLE_DCHECK_NE(&from, _this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.resource_logs_.MergeFrom(from._impl_.resource_logs_);
  _this->_internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void ExportLogsServiceRequest::CopyFrom(const ExportLogsServiceRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ExportLogsServiceRequest::IsInitialized() const {
  return true;
}

void ExportLogsServiceRequest::InternalSwap(ExportLogsServiceRequest* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.resource_logs_.InternalSwap(&other->_impl_.resource_logs_);
}

::PROTOBUF_NAMESPACE_ID::Metadata ExportLogsServiceRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_getter, &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_once,
      file_level_metadata_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto[0]);
}

// ===================================================================

class ExportLogsServiceResponse::_Internal {
 public:
  static const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess& partial_success(const ExportLogsServiceResponse* msg);
};

const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess&
ExportLogsServiceResponse::_Internal::partial_success(const ExportLogsServiceResponse* msg) {
  return *msg->_impl_.partial_success_;
}
ExportLogsServiceResponse::ExportLogsServiceResponse(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor(arena, is_message_owned);
  // @@protoc_insertion_point(arena_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
}
ExportLogsServiceResponse::ExportLogsServiceResponse(const ExportLogsServiceResponse& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  ExportLogsServiceResponse* const _this = this; (void)_this;
  new (&_impl_) Impl_{
      decltype(_impl_.partial_success_){nullptr}
    , /*decltype(_impl_._cached_size_)*/{}};

  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  if (from._internal_has_partial_success()) {
    _this->_impl_.partial_success_ = new ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess(*from._impl_.partial_success_);
  }
  // @@protoc_insertion_point(copy_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
}

inline void ExportLogsServiceResponse::SharedCtor(
    ::_pb::Arena* arena, bool is_message_owned) {
  (void)arena;
  (void)is_message_owned;
  new (&_impl_) Impl_{
      decltype(_impl_.partial_success_){nullptr}
    , /*decltype(_impl_._cached_size_)*/{}
  };
}

ExportLogsServiceResponse::~ExportLogsServiceResponse() {
  // @@protoc_insertion_point(destructor:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void ExportLogsServiceResponse::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  if (this != internal_default_instance()) delete _impl_.partial_success_;
}

void ExportLogsServiceResponse::SetCachedSize(int size) const {
  _impl_._cached_size_.Set(size);
}

void ExportLogsServiceResponse::Clear() {
// @@protoc_insertion_point(message_clear_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  if (GetArenaForAllocation() == nullptr && _impl_.partial_success_ != nullptr) {
    delete _impl_.partial_success_;
  }
  _impl_.partial_success_ = nullptr;
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* ExportLogsServiceResponse::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      // .opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess partial_success = 1;
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 10)) {
          ptr = ctx->ParseMessage(_internal_mutable_partial_success(), ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }  // switch
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }  // while
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* ExportLogsServiceResponse::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  // .opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess partial_success = 1;
  if (this->_internal_has_partial_success()) {
    target = ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::
      InternalWriteMessage(1, _Internal::partial_success(this),
        _Internal::partial_success(this).GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  return target;
}

size_t ExportLogsServiceResponse::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // .opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess partial_success = 1;
  if (this->_internal_has_partial_success()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::MessageSize(
        *_impl_.partial_success_);
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData ExportLogsServiceResponse::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSourceCheck,
    ExportLogsServiceResponse::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*ExportLogsServiceResponse::GetClassData() const { return &_class_data_; }


void ExportLogsServiceResponse::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg) {
  auto* const _this = static_cast<ExportLogsServiceResponse*>(&to_msg);
  auto& from = static_cast<const ExportLogsServiceResponse&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  GOOGLE_DCHECK_NE(&from, _this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_has_partial_success()) {
    _this->_internal_mutable_partial_success()->::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess::MergeFrom(
        from._internal_partial_success());
  }
  _this->_internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void ExportLogsServiceResponse::CopyFrom(const ExportLogsServiceResponse& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ExportLogsServiceResponse::IsInitialized() const {
  return true;
}

void ExportLogsServiceResponse::InternalSwap(ExportLogsServiceResponse* other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_.partial_success_, other->_impl_.partial_success_);
}

::PROTOBUF_NAMESPACE_ID::Metadata ExportLogsServiceResponse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_getter, &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_once,
      file_level_metadata_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto[1]);
}

// ===================================================================

class ExportLogsPartialSuccess::_Internal {
 public:
};

ExportLogsPartialSuccess::ExportLogsPartialSuccess(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor(arena, is_message_owned);
  // @@protoc_insertion_point(arena_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
}
ExportLogsPartialSuccess::ExportLogsPartialSuccess(const ExportLogsPartialSuccess& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  ExportLogsPartialSuccess* const _this = this; (void)_this;
  new (&_impl_) Impl_{
      decltype(_impl_.error_message_){}
    , decltype(_impl_.rejected_log_records_){}
    , /*decltype(_impl_._cached_size_)*/{}};

  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  _impl_.error_message_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.error_message_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (!from._internal_error_message().empty()) {
    _this->_impl_.error_message_.Set(from._internal_error_message(), 
      _this->GetArenaForAllocation());
  }
  _this->_impl_.rejected_log_records_ = from._impl_.rejected_log_records_;
  // @@protoc_insertion_point(copy_constructor:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
}

inline void ExportLogsPartialSuccess::SharedCtor(
    ::_pb::Arena* arena, bool is_message_owned) {
  (void)arena;
  (void)is_message_owned;
  new (&_impl_) Impl_{
      decltype(_impl_.error_message_){}
    , decltype(_impl_.rejected_log_records_){int64_t{0}}
    , /*decltype(_impl_._cached_size_)*/{}
  };
  _impl_.error_message_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.error_message_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
}

ExportLogsPartialSuccess::~ExportLogsPartialSuccess() {
  // @@protoc_insertion_point(destructor:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void ExportLogsPartialSuccess::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  _impl_.error_message_.Destroy();
}

void ExportLogsPartialSuccess::SetCachedSize(int size) const {
  _impl_._cached_size_.Set(size);
}

void ExportLogsPartialSuccess::Clear() {
// @@protoc_insertion_point(message_clear_start:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.error_message_.ClearToEmpty();
  _impl_.rejected_log_records_ = int64_t{0};
  _internal_metadata_.Clear<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>();
}

const char* ExportLogsPartialSuccess::_InternalParse(const char* ptr, ::_pbi::ParseContext* ctx) {
#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure
  while (!ctx->Done(&ptr)) {
    uint32_t tag;
    ptr = ::_pbi::ReadTag(ptr, &tag);
    switch (tag >> 3) {
      // int64 rejected_log_records = 1;
      case 1:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 8)) {
          _impl_.rejected_log_records_ = ::PROTOBUF_NAMESPACE_ID::internal::ReadVarint64(&ptr);
          CHK_(ptr);
        } else
          goto handle_unusual;
        continue;
      // string error_message = 2;
      case 2:
        if (PROTOBUF_PREDICT_TRUE(static_cast<uint8_t>(tag) == 18)) {
          auto str = _internal_mutable_error_message();
          ptr = ::_pbi::InlineGreedyStringParser(str, ptr, ctx);
          CHK_(ptr);
          CHK_(::_pbi::VerifyUTF8(str, "opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message"));
        } else
          goto handle_unusual;
        continue;
      default:
        goto handle_unusual;
    }  // switch
  handle_unusual:
    if ((tag == 0) || ((tag & 7) == 4)) {
      CHK_(ptr);
      ctx->SetLastTag(tag);
      goto message_done;
    }
    ptr = UnknownFieldParse(
        tag,
        _internal_metadata_.mutable_unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(),
        ptr, ctx);
    CHK_(ptr != nullptr);
  }  // while
message_done:
  return ptr;
failure:
  ptr = nullptr;
  goto message_done;
#undef CHK_
}

uint8_t* ExportLogsPartialSuccess::_InternalSerialize(
    uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  // int64 rejected_log_records = 1;
  if (this->_internal_rejected_log_records() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteInt64ToArray(1, this->_internal_rejected_log_records(), target);
  }

  // string error_message = 2;
  if (!this->_internal_error_message().empty()) {
    ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::VerifyUtf8String(
      this->_internal_error_message().data(), static_cast<int>(this->_internal_error_message().length()),
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::SERIALIZE,
      "opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message");
    target = stream->WriteStringMaybeAliased(
        2, this->_internal_error_message(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
        _internal_metadata_.unknown_fields<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(::PROTOBUF_NAMESPACE_ID::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  return target;
}

size_t ExportLogsPartialSuccess::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  size_t total_size = 0;

  uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string error_message = 2;
  if (!this->_internal_error_message().empty()) {
    total_size += 1 +
      ::PROTOBUF_NAMESPACE_ID::internal::WireFormatLite::StringSize(
        this->_internal_error_message());
  }

  // int64 rejected_log_records = 1;
  if (this->_internal_rejected_log_records() != 0) {
    total_size += ::_pbi::WireFormatLite::Int64SizePlusOne(this->_internal_rejected_log_records());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::PROTOBUF_NAMESPACE_ID::Message::ClassData ExportLogsPartialSuccess::_class_data_ = {
    ::PROTOBUF_NAMESPACE_ID::Message::CopyWithSourceCheck,
    ExportLogsPartialSuccess::MergeImpl
};
const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*ExportLogsPartialSuccess::GetClassData() const { return &_class_data_; }


void ExportLogsPartialSuccess::MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg) {
  auto* const _this = static_cast<ExportLogsPartialSuccess*>(&to_msg);
  auto& from = static_cast<const ExportLogsPartialSuccess&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  GOOGLE_DCHECK_NE(&from, _this);
  uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_error_message().empty()) {
    _this->_internal_set_error_message(from._internal_error_message());
  }
  if (from._internal_rejected_log_records() != 0) {
    _this->_internal_set_rejected_log_records(from._internal_rejected_log_records());
  }
  _this->_internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
}

void ExportLogsPartialSuccess::CopyFrom(const ExportLogsPartialSuccess& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ExportLogsPartialSuccess::IsInitialized() const {
  return true;
}

void ExportLogsPartialSuccess::InternalSwap(ExportLogsPartialSuccess* other) {
  using std::swap;
  auto* lhs_arena = GetArenaForAllocation();
  auto* rhs_arena = other->GetArenaForAllocation();
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr::InternalSwap(
      &_impl_.error_message_, lhs_arena,
      &other->_impl_.error_message_, rhs_arena
  );
  swap(_impl_.rejected_log_records_, other->_impl_.rejected_log_records_);
}

::PROTOBUF_NAMESPACE_ID::Metadata ExportLogsPartialSuccess::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_getter, &descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto_once,
      file_level_metadata_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto[2]);
}

// @@protoc_insertion_point(namespace_scope)
}  // namespace v1
}  // namespace logs
}  // namespace collector
}  // namespace proto
}  // namespace opentelemetry
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest*
Arena::CreateMaybeMessage< ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest >(Arena* arena) {
  return Arena::CreateMessageInternal< ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest >(arena);
}
template<> PROTOBUF_NOINLINE ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse*
Arena::CreateMaybeMessage< ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse >(Arena* arena) {
  return Arena::CreateMessageInternal< ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse >(arena);
}
template<> PROTOBUF_NOINLINE ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess*
Arena::CreateMaybeMessage< ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess >(Arena* arena) {
  return Arena::CreateMessageInternal< ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
