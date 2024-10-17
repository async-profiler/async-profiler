package one.profiler.test;

public enum SkipReason {
    Disabled,
    ArchMismatch,
    OsMismatch,
    JvmMismatch,
    JvmVersionMismatch,
    SkipByClassName,
    SkipByMethodName,
}
