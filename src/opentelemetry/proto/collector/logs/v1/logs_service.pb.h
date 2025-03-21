// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: opentelemetry/proto/collector/logs/v1/logs_service.proto

#ifndef GOOGLE_PROTOBUF_INCLUDED_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto
#define GOOGLE_PROTOBUF_INCLUDED_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto

#include <limits>
#include <string>

#include <google/protobuf/port_def.inc>
#if PROTOBUF_VERSION < 3021000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers. Please update
#error your headers.
#endif
#if 3021012 < PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers. Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/port_undef.inc>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
#include "opentelemetry/proto/logs/v1/logs.pb.h"
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>
#define PROTOBUF_INTERNAL_EXPORT_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto
PROTOBUF_NAMESPACE_OPEN
namespace internal {
class AnyMetadata;
}  // namespace internal
PROTOBUF_NAMESPACE_CLOSE

// Internal implementation detail -- do not use these members.
struct TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto {
  static const uint32_t offsets[];
};
extern const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable descriptor_table_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto;
namespace opentelemetry {
namespace proto {
namespace collector {
namespace logs {
namespace v1 {
class ExportLogsPartialSuccess;
struct ExportLogsPartialSuccessDefaultTypeInternal;
extern ExportLogsPartialSuccessDefaultTypeInternal _ExportLogsPartialSuccess_default_instance_;
class ExportLogsServiceRequest;
struct ExportLogsServiceRequestDefaultTypeInternal;
extern ExportLogsServiceRequestDefaultTypeInternal _ExportLogsServiceRequest_default_instance_;
class ExportLogsServiceResponse;
struct ExportLogsServiceResponseDefaultTypeInternal;
extern ExportLogsServiceResponseDefaultTypeInternal _ExportLogsServiceResponse_default_instance_;
}  // namespace v1
}  // namespace logs
}  // namespace collector
}  // namespace proto
}  // namespace opentelemetry
PROTOBUF_NAMESPACE_OPEN
template<> ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* Arena::CreateMaybeMessage<::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess>(Arena*);
template<> ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest* Arena::CreateMaybeMessage<::opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest>(Arena*);
template<> ::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse* Arena::CreateMaybeMessage<::opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse>(Arena*);
PROTOBUF_NAMESPACE_CLOSE
namespace opentelemetry {
namespace proto {
namespace collector {
namespace logs {
namespace v1 {

// ===================================================================

class ExportLogsServiceRequest final :
    public ::PROTOBUF_NAMESPACE_ID::Message /* @@protoc_insertion_point(class_definition:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest) */ {
 public:
  inline ExportLogsServiceRequest() : ExportLogsServiceRequest(nullptr) {}
  ~ExportLogsServiceRequest() override;
  explicit PROTOBUF_CONSTEXPR ExportLogsServiceRequest(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  ExportLogsServiceRequest(const ExportLogsServiceRequest& from);
  ExportLogsServiceRequest(ExportLogsServiceRequest&& from) noexcept
    : ExportLogsServiceRequest() {
    *this = ::std::move(from);
  }

  inline ExportLogsServiceRequest& operator=(const ExportLogsServiceRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline ExportLogsServiceRequest& operator=(ExportLogsServiceRequest&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetOwningArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::PROTOBUF_NAMESPACE_ID::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const ExportLogsServiceRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const ExportLogsServiceRequest* internal_default_instance() {
    return reinterpret_cast<const ExportLogsServiceRequest*>(
               &_ExportLogsServiceRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  friend void swap(ExportLogsServiceRequest& a, ExportLogsServiceRequest& b) {
    a.Swap(&b);
  }
  inline void Swap(ExportLogsServiceRequest* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() != nullptr &&
        GetOwningArena() == other->GetOwningArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() == other->GetOwningArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ExportLogsServiceRequest* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ExportLogsServiceRequest* New(::PROTOBUF_NAMESPACE_ID::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<ExportLogsServiceRequest>(arena);
  }
  using ::PROTOBUF_NAMESPACE_ID::Message::CopyFrom;
  void CopyFrom(const ExportLogsServiceRequest& from);
  using ::PROTOBUF_NAMESPACE_ID::Message::MergeFrom;
  void MergeFrom( const ExportLogsServiceRequest& from) {
    ExportLogsServiceRequest::MergeImpl(*this, from);
  }
  private:
  static void MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg);
  public:
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  uint8_t* _InternalSerialize(
      uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const final { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::PROTOBUF_NAMESPACE_ID::Arena* arena, bool is_message_owned);
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(ExportLogsServiceRequest* other);

  private:
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest";
  }
  protected:
  explicit ExportLogsServiceRequest(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  public:

  static const ClassData _class_data_;
  const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*GetClassData() const final;

  ::PROTOBUF_NAMESPACE_ID::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kResourceLogsFieldNumber = 1,
  };
  // repeated .opentelemetry.proto.logs.v1.ResourceLogs resource_logs = 1;
  int resource_logs_size() const;
  private:
  int _internal_resource_logs_size() const;
  public:
  void clear_resource_logs();
  ::opentelemetry::proto::logs::v1::ResourceLogs* mutable_resource_logs(int index);
  ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField< ::opentelemetry::proto::logs::v1::ResourceLogs >*
      mutable_resource_logs();
  private:
  const ::opentelemetry::proto::logs::v1::ResourceLogs& _internal_resource_logs(int index) const;
  ::opentelemetry::proto::logs::v1::ResourceLogs* _internal_add_resource_logs();
  public:
  const ::opentelemetry::proto::logs::v1::ResourceLogs& resource_logs(int index) const;
  ::opentelemetry::proto::logs::v1::ResourceLogs* add_resource_logs();
  const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField< ::opentelemetry::proto::logs::v1::ResourceLogs >&
      resource_logs() const;

  // @@protoc_insertion_point(class_scope:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  struct Impl_ {
    ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField< ::opentelemetry::proto::logs::v1::ResourceLogs > resource_logs_;
    mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto;
};
// -------------------------------------------------------------------

class ExportLogsServiceResponse final :
    public ::PROTOBUF_NAMESPACE_ID::Message /* @@protoc_insertion_point(class_definition:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse) */ {
 public:
  inline ExportLogsServiceResponse() : ExportLogsServiceResponse(nullptr) {}
  ~ExportLogsServiceResponse() override;
  explicit PROTOBUF_CONSTEXPR ExportLogsServiceResponse(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  ExportLogsServiceResponse(const ExportLogsServiceResponse& from);
  ExportLogsServiceResponse(ExportLogsServiceResponse&& from) noexcept
    : ExportLogsServiceResponse() {
    *this = ::std::move(from);
  }

  inline ExportLogsServiceResponse& operator=(const ExportLogsServiceResponse& from) {
    CopyFrom(from);
    return *this;
  }
  inline ExportLogsServiceResponse& operator=(ExportLogsServiceResponse&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetOwningArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::PROTOBUF_NAMESPACE_ID::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const ExportLogsServiceResponse& default_instance() {
    return *internal_default_instance();
  }
  static inline const ExportLogsServiceResponse* internal_default_instance() {
    return reinterpret_cast<const ExportLogsServiceResponse*>(
               &_ExportLogsServiceResponse_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    1;

  friend void swap(ExportLogsServiceResponse& a, ExportLogsServiceResponse& b) {
    a.Swap(&b);
  }
  inline void Swap(ExportLogsServiceResponse* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() != nullptr &&
        GetOwningArena() == other->GetOwningArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() == other->GetOwningArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ExportLogsServiceResponse* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ExportLogsServiceResponse* New(::PROTOBUF_NAMESPACE_ID::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<ExportLogsServiceResponse>(arena);
  }
  using ::PROTOBUF_NAMESPACE_ID::Message::CopyFrom;
  void CopyFrom(const ExportLogsServiceResponse& from);
  using ::PROTOBUF_NAMESPACE_ID::Message::MergeFrom;
  void MergeFrom( const ExportLogsServiceResponse& from) {
    ExportLogsServiceResponse::MergeImpl(*this, from);
  }
  private:
  static void MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg);
  public:
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  uint8_t* _InternalSerialize(
      uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const final { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::PROTOBUF_NAMESPACE_ID::Arena* arena, bool is_message_owned);
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(ExportLogsServiceResponse* other);

  private:
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse";
  }
  protected:
  explicit ExportLogsServiceResponse(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  public:

  static const ClassData _class_data_;
  const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*GetClassData() const final;

  ::PROTOBUF_NAMESPACE_ID::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kPartialSuccessFieldNumber = 1,
  };
  // .opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess partial_success = 1;
  bool has_partial_success() const;
  private:
  bool _internal_has_partial_success() const;
  public:
  void clear_partial_success();
  const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess& partial_success() const;
  PROTOBUF_NODISCARD ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* release_partial_success();
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* mutable_partial_success();
  void set_allocated_partial_success(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* partial_success);
  private:
  const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess& _internal_partial_success() const;
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* _internal_mutable_partial_success();
  public:
  void unsafe_arena_set_allocated_partial_success(
      ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* partial_success);
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* unsafe_arena_release_partial_success();

  // @@protoc_insertion_point(class_scope:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  struct Impl_ {
    ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* partial_success_;
    mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto;
};
// -------------------------------------------------------------------

class ExportLogsPartialSuccess final :
    public ::PROTOBUF_NAMESPACE_ID::Message /* @@protoc_insertion_point(class_definition:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess) */ {
 public:
  inline ExportLogsPartialSuccess() : ExportLogsPartialSuccess(nullptr) {}
  ~ExportLogsPartialSuccess() override;
  explicit PROTOBUF_CONSTEXPR ExportLogsPartialSuccess(::PROTOBUF_NAMESPACE_ID::internal::ConstantInitialized);

  ExportLogsPartialSuccess(const ExportLogsPartialSuccess& from);
  ExportLogsPartialSuccess(ExportLogsPartialSuccess&& from) noexcept
    : ExportLogsPartialSuccess() {
    *this = ::std::move(from);
  }

  inline ExportLogsPartialSuccess& operator=(const ExportLogsPartialSuccess& from) {
    CopyFrom(from);
    return *this;
  }
  inline ExportLogsPartialSuccess& operator=(ExportLogsPartialSuccess&& from) noexcept {
    if (this == &from) return *this;
    if (GetOwningArena() == from.GetOwningArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetOwningArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::PROTOBUF_NAMESPACE_ID::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::PROTOBUF_NAMESPACE_ID::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const ExportLogsPartialSuccess& default_instance() {
    return *internal_default_instance();
  }
  static inline const ExportLogsPartialSuccess* internal_default_instance() {
    return reinterpret_cast<const ExportLogsPartialSuccess*>(
               &_ExportLogsPartialSuccess_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    2;

  friend void swap(ExportLogsPartialSuccess& a, ExportLogsPartialSuccess& b) {
    a.Swap(&b);
  }
  inline void Swap(ExportLogsPartialSuccess* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() != nullptr &&
        GetOwningArena() == other->GetOwningArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetOwningArena() == other->GetOwningArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::PROTOBUF_NAMESPACE_ID::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(ExportLogsPartialSuccess* other) {
    if (other == this) return;
    GOOGLE_DCHECK(GetOwningArena() == other->GetOwningArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ExportLogsPartialSuccess* New(::PROTOBUF_NAMESPACE_ID::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<ExportLogsPartialSuccess>(arena);
  }
  using ::PROTOBUF_NAMESPACE_ID::Message::CopyFrom;
  void CopyFrom(const ExportLogsPartialSuccess& from);
  using ::PROTOBUF_NAMESPACE_ID::Message::MergeFrom;
  void MergeFrom( const ExportLogsPartialSuccess& from) {
    ExportLogsPartialSuccess::MergeImpl(*this, from);
  }
  private:
  static void MergeImpl(::PROTOBUF_NAMESPACE_ID::Message& to_msg, const ::PROTOBUF_NAMESPACE_ID::Message& from_msg);
  public:
  PROTOBUF_ATTRIBUTE_REINITIALIZES void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  const char* _InternalParse(const char* ptr, ::PROTOBUF_NAMESPACE_ID::internal::ParseContext* ctx) final;
  uint8_t* _InternalSerialize(
      uint8_t* target, ::PROTOBUF_NAMESPACE_ID::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const final { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::PROTOBUF_NAMESPACE_ID::Arena* arena, bool is_message_owned);
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(ExportLogsPartialSuccess* other);

  private:
  friend class ::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata;
  static ::PROTOBUF_NAMESPACE_ID::StringPiece FullMessageName() {
    return "opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess";
  }
  protected:
  explicit ExportLogsPartialSuccess(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                       bool is_message_owned = false);
  public:

  static const ClassData _class_data_;
  const ::PROTOBUF_NAMESPACE_ID::Message::ClassData*GetClassData() const final;

  ::PROTOBUF_NAMESPACE_ID::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kErrorMessageFieldNumber = 2,
    kRejectedLogRecordsFieldNumber = 1,
  };
  // string error_message = 2;
  void clear_error_message();
  const std::string& error_message() const;
  template <typename ArgT0 = const std::string&, typename... ArgT>
  void set_error_message(ArgT0&& arg0, ArgT... args);
  std::string* mutable_error_message();
  PROTOBUF_NODISCARD std::string* release_error_message();
  void set_allocated_error_message(std::string* error_message);
  private:
  const std::string& _internal_error_message() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_error_message(const std::string& value);
  std::string* _internal_mutable_error_message();
  public:

  // int64 rejected_log_records = 1;
  void clear_rejected_log_records();
  int64_t rejected_log_records() const;
  void set_rejected_log_records(int64_t value);
  private:
  int64_t _internal_rejected_log_records() const;
  void _internal_set_rejected_log_records(int64_t value);
  public:

  // @@protoc_insertion_point(class_scope:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess)
 private:
  class _Internal;

  template <typename T> friend class ::PROTOBUF_NAMESPACE_ID::Arena::InternalHelper;
  typedef void InternalArenaConstructable_;
  typedef void DestructorSkippable_;
  struct Impl_ {
    ::PROTOBUF_NAMESPACE_ID::internal::ArenaStringPtr error_message_;
    int64_t rejected_log_records_;
    mutable ::PROTOBUF_NAMESPACE_ID::internal::CachedSize _cached_size_;
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto;
};
// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// ExportLogsServiceRequest

// repeated .opentelemetry.proto.logs.v1.ResourceLogs resource_logs = 1;
inline int ExportLogsServiceRequest::_internal_resource_logs_size() const {
  return _impl_.resource_logs_.size();
}
inline int ExportLogsServiceRequest::resource_logs_size() const {
  return _internal_resource_logs_size();
}
inline ::opentelemetry::proto::logs::v1::ResourceLogs* ExportLogsServiceRequest::mutable_resource_logs(int index) {
  // @@protoc_insertion_point(field_mutable:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest.resource_logs)
  return _impl_.resource_logs_.Mutable(index);
}
inline ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField< ::opentelemetry::proto::logs::v1::ResourceLogs >*
ExportLogsServiceRequest::mutable_resource_logs() {
  // @@protoc_insertion_point(field_mutable_list:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest.resource_logs)
  return &_impl_.resource_logs_;
}
inline const ::opentelemetry::proto::logs::v1::ResourceLogs& ExportLogsServiceRequest::_internal_resource_logs(int index) const {
  return _impl_.resource_logs_.Get(index);
}
inline const ::opentelemetry::proto::logs::v1::ResourceLogs& ExportLogsServiceRequest::resource_logs(int index) const {
  // @@protoc_insertion_point(field_get:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest.resource_logs)
  return _internal_resource_logs(index);
}
inline ::opentelemetry::proto::logs::v1::ResourceLogs* ExportLogsServiceRequest::_internal_add_resource_logs() {
  return _impl_.resource_logs_.Add();
}
inline ::opentelemetry::proto::logs::v1::ResourceLogs* ExportLogsServiceRequest::add_resource_logs() {
  ::opentelemetry::proto::logs::v1::ResourceLogs* _add = _internal_add_resource_logs();
  // @@protoc_insertion_point(field_add:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest.resource_logs)
  return _add;
}
inline const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField< ::opentelemetry::proto::logs::v1::ResourceLogs >&
ExportLogsServiceRequest::resource_logs() const {
  // @@protoc_insertion_point(field_list:opentelemetry.proto.collector.logs.v1.ExportLogsServiceRequest.resource_logs)
  return _impl_.resource_logs_;
}

// -------------------------------------------------------------------

// ExportLogsServiceResponse

// .opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess partial_success = 1;
inline bool ExportLogsServiceResponse::_internal_has_partial_success() const {
  return this != internal_default_instance() && _impl_.partial_success_ != nullptr;
}
inline bool ExportLogsServiceResponse::has_partial_success() const {
  return _internal_has_partial_success();
}
inline void ExportLogsServiceResponse::clear_partial_success() {
  if (GetArenaForAllocation() == nullptr && _impl_.partial_success_ != nullptr) {
    delete _impl_.partial_success_;
  }
  _impl_.partial_success_ = nullptr;
}
inline const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess& ExportLogsServiceResponse::_internal_partial_success() const {
  const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* p = _impl_.partial_success_;
  return p != nullptr ? *p : reinterpret_cast<const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess&>(
      ::opentelemetry::proto::collector::logs::v1::_ExportLogsPartialSuccess_default_instance_);
}
inline const ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess& ExportLogsServiceResponse::partial_success() const {
  // @@protoc_insertion_point(field_get:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse.partial_success)
  return _internal_partial_success();
}
inline void ExportLogsServiceResponse::unsafe_arena_set_allocated_partial_success(
    ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* partial_success) {
  if (GetArenaForAllocation() == nullptr) {
    delete reinterpret_cast<::PROTOBUF_NAMESPACE_ID::MessageLite*>(_impl_.partial_success_);
  }
  _impl_.partial_success_ = partial_success;
  if (partial_success) {
    
  } else {
    
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse.partial_success)
}
inline ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* ExportLogsServiceResponse::release_partial_success() {
  
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* temp = _impl_.partial_success_;
  _impl_.partial_success_ = nullptr;
#ifdef PROTOBUF_FORCE_COPY_IN_RELEASE
  auto* old =  reinterpret_cast<::PROTOBUF_NAMESPACE_ID::MessageLite*>(temp);
  temp = ::PROTOBUF_NAMESPACE_ID::internal::DuplicateIfNonNull(temp);
  if (GetArenaForAllocation() == nullptr) { delete old; }
#else  // PROTOBUF_FORCE_COPY_IN_RELEASE
  if (GetArenaForAllocation() != nullptr) {
    temp = ::PROTOBUF_NAMESPACE_ID::internal::DuplicateIfNonNull(temp);
  }
#endif  // !PROTOBUF_FORCE_COPY_IN_RELEASE
  return temp;
}
inline ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* ExportLogsServiceResponse::unsafe_arena_release_partial_success() {
  // @@protoc_insertion_point(field_release:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse.partial_success)
  
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* temp = _impl_.partial_success_;
  _impl_.partial_success_ = nullptr;
  return temp;
}
inline ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* ExportLogsServiceResponse::_internal_mutable_partial_success() {
  
  if (_impl_.partial_success_ == nullptr) {
    auto* p = CreateMaybeMessage<::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess>(GetArenaForAllocation());
    _impl_.partial_success_ = p;
  }
  return _impl_.partial_success_;
}
inline ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* ExportLogsServiceResponse::mutable_partial_success() {
  ::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* _msg = _internal_mutable_partial_success();
  // @@protoc_insertion_point(field_mutable:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse.partial_success)
  return _msg;
}
inline void ExportLogsServiceResponse::set_allocated_partial_success(::opentelemetry::proto::collector::logs::v1::ExportLogsPartialSuccess* partial_success) {
  ::PROTOBUF_NAMESPACE_ID::Arena* message_arena = GetArenaForAllocation();
  if (message_arena == nullptr) {
    delete _impl_.partial_success_;
  }
  if (partial_success) {
    ::PROTOBUF_NAMESPACE_ID::Arena* submessage_arena =
        ::PROTOBUF_NAMESPACE_ID::Arena::InternalGetOwningArena(partial_success);
    if (message_arena != submessage_arena) {
      partial_success = ::PROTOBUF_NAMESPACE_ID::internal::GetOwnedMessage(
          message_arena, partial_success, submessage_arena);
    }
    
  } else {
    
  }
  _impl_.partial_success_ = partial_success;
  // @@protoc_insertion_point(field_set_allocated:opentelemetry.proto.collector.logs.v1.ExportLogsServiceResponse.partial_success)
}

// -------------------------------------------------------------------

// ExportLogsPartialSuccess

// int64 rejected_log_records = 1;
inline void ExportLogsPartialSuccess::clear_rejected_log_records() {
  _impl_.rejected_log_records_ = int64_t{0};
}
inline int64_t ExportLogsPartialSuccess::_internal_rejected_log_records() const {
  return _impl_.rejected_log_records_;
}
inline int64_t ExportLogsPartialSuccess::rejected_log_records() const {
  // @@protoc_insertion_point(field_get:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.rejected_log_records)
  return _internal_rejected_log_records();
}
inline void ExportLogsPartialSuccess::_internal_set_rejected_log_records(int64_t value) {
  
  _impl_.rejected_log_records_ = value;
}
inline void ExportLogsPartialSuccess::set_rejected_log_records(int64_t value) {
  _internal_set_rejected_log_records(value);
  // @@protoc_insertion_point(field_set:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.rejected_log_records)
}

// string error_message = 2;
inline void ExportLogsPartialSuccess::clear_error_message() {
  _impl_.error_message_.ClearToEmpty();
}
inline const std::string& ExportLogsPartialSuccess::error_message() const {
  // @@protoc_insertion_point(field_get:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message)
  return _internal_error_message();
}
template <typename ArgT0, typename... ArgT>
inline PROTOBUF_ALWAYS_INLINE
void ExportLogsPartialSuccess::set_error_message(ArgT0&& arg0, ArgT... args) {
 
 _impl_.error_message_.Set(static_cast<ArgT0 &&>(arg0), args..., GetArenaForAllocation());
  // @@protoc_insertion_point(field_set:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message)
}
inline std::string* ExportLogsPartialSuccess::mutable_error_message() {
  std::string* _s = _internal_mutable_error_message();
  // @@protoc_insertion_point(field_mutable:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message)
  return _s;
}
inline const std::string& ExportLogsPartialSuccess::_internal_error_message() const {
  return _impl_.error_message_.Get();
}
inline void ExportLogsPartialSuccess::_internal_set_error_message(const std::string& value) {
  
  _impl_.error_message_.Set(value, GetArenaForAllocation());
}
inline std::string* ExportLogsPartialSuccess::_internal_mutable_error_message() {
  
  return _impl_.error_message_.Mutable(GetArenaForAllocation());
}
inline std::string* ExportLogsPartialSuccess::release_error_message() {
  // @@protoc_insertion_point(field_release:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message)
  return _impl_.error_message_.Release();
}
inline void ExportLogsPartialSuccess::set_allocated_error_message(std::string* error_message) {
  if (error_message != nullptr) {
    
  } else {
    
  }
  _impl_.error_message_.SetAllocated(error_message, GetArenaForAllocation());
#ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (_impl_.error_message_.IsDefault()) {
    _impl_.error_message_.Set("", GetArenaForAllocation());
  }
#endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:opentelemetry.proto.collector.logs.v1.ExportLogsPartialSuccess.error_message)
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__
// -------------------------------------------------------------------

// -------------------------------------------------------------------


// @@protoc_insertion_point(namespace_scope)

}  // namespace v1
}  // namespace logs
}  // namespace collector
}  // namespace proto
}  // namespace opentelemetry

// @@protoc_insertion_point(global_scope)

#include <google/protobuf/port_undef.inc>
#endif  // GOOGLE_PROTOBUF_INCLUDED_GOOGLE_PROTOBUF_INCLUDED_opentelemetry_2fproto_2fcollector_2flogs_2fv1_2flogs_5fservice_2eproto
