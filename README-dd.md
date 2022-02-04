# Async Profiler (Datadog)

## Building Local Artifacts
In order to ease up consuming of the async-profiler library in downstream projects (eg. dd-trace-java) it is possible to quickly build a maven artifact (jar) which can replace the stable dependency to test the changes.

### Prerequisites
The local build requires docker and Java 11 to be installed.

### Build script
Use `./datadog/scripts/build_locally.sh` to build linux-x64 only maven artifact.

After running this script you should see something like
```shell
jaroslav.bachorik@COMP-C02FJ0PSMD6V async-profiler % ./datadog/scripts/build_locally.sh
=== Building Async Profiler
==    Version     : 2.6-DD-jb_local_artifact-bc38fb7712459603349d7a36a90c9d02611a450d
==    Architecture: linux-amd64
==    With tests  : no
-> Building native library
-> Building maven artifact
-> Build done : Artifacts available |
*  file:///tmp/ap-tools-2.6-DD-jb_local_artifact-bc38fb7712459603349d7a36a90c9d02611a450d.jar
```

The artifact version contain the encoded branch name and the HEAD commit hash - which makes it easily identifiable. The actual path of the built artifact depends on your system but you can easily copy-paste it around.

#### Supported arguments
The build script support the following arguments:
- `-a <architecture>` 
  - one of `linux-x64`, `linux-x64-musl` or `linux-arm64` (defaults to `linux-x64`)
- `-f`
  - force docker image rebuild
- `-t`
  - force test run
- `-h`
  - show help

### Consuming the artifact
For dd-trace-java you just need to set the `AP_TOOLS_URL` environment variable.
Eg. you can run the gradle build like this - `AP_TOOLS_URL=file:///tmp/ap-tools-2.6-DD-jb_local_artifact-bc38fb7712459603349d7a36a90c9d02611a450d.jar ./gradlew clean :dd-java-agent:shadowJar` - which will result in a custom `dd-java-agent.jar` build containing your test version of async profiler.