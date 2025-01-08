# Output Formats

async-profiler currently supports the following output formats:

- `collapsed` - This is a collection of call stacks, where each line is a semicolon separated list of frames followed
  by a counter. This is used by the FlameGraph script to generate the FlameGraph visualization of the profile data.

  ```
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult 21
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult;java/io/DataOutputStream.writeInt 1
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult;java/io/DataOutputStream.writeInt;java/io/ByteArrayOutputStream.write 5
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult;java/io/DataOutputStream.writeUTF;java/io/DataOutputStream.writeUTF 12
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult;java/io/DataOutputStream.writeUTF;java/io/DataOutputStream.writeUTF;java/lang/String.length 3
  FileConverter.main;FileConverter.convertFile;FileConverter.saveResult;java/io/DataOutputStream.writeUTF;java/io/DataOutputStream.writeUTF;java/io/DataOutputStream.write 6
  start_thread;thread_native_entry;Thread::call_run;VMThread::run;VMThread::inner_execute;VMThread::evaluate_operation;VM_Operation::evaluate;VM_GenCollectForAllocation::doit;GenCollectedHeap::satisfy_failed_allocation;GenCollectedHeap::do_collection;GenCollectedHeap::collect_generation;DefNewGeneration::collect;DefNewGeneration::FastEvacuateFollowersClosure::do_void 12
  start_thread;thread_native_entry;Thread::call_run;VMThread::run;VMThread::inner_execute;VMThread::evaluate_operation;VM_Operation::evaluate;VM_GenCollectForAllocation::doit;GenCollectedHeap::satisfy_failed_allocation;GenCollectedHeap::do_collection;GenCollectedHeap::collect_generation;DefNewGeneration::collect;DefNewGeneration::FastEvacuateFollowersClosure::do_void;void ContiguousSpace::oop_since_save_marks_iterate<DefNewScanClosure> 1
  ```

- `flamegraph` - FlameGraph is a hierarchical representation of call traces of the profiled software in a color coded
  format. Read more on the [interpretation](FlamegraphInterpretation.md) of FlameGraphs.
  [![FlameGraph](/.assets/images/flamegraph.png)](https://htmlpreview.github.io/?https://github.com/async-profiler/async-profiler/blob/master/.assets/html/flamegraph.html)

- `tree` - Profile output generated in HTML format showing a tree view of resource usage beginning with the call stack
  with the highest resource usage and then showing other call stacks in descending order of resource usage. Expanding a
  parent frame follows the same hierarchical representation within that frame.
  ![Tree](/.assets/images/treeview_example.png)

- `text` - If no output format is specified with `-o` and filename has no extension provided, profiled output is
  generated in text format.

  ```
  --- Execution profile ---
  Total samples       : 733

  --- 8208 bytes (19.58%), 1 sample
  [ 0] byte[]
  [ 1] java.util.jar.Manifest$FastInputStream.<init>
  [ 2] java.util.jar.Manifest$FastInputStream.<init>
  [ 3] java.util.jar.Manifest.read
  [ 4] java.util.jar.Manifest.<init>
  [ 5] java.util.jar.Manifest.<init>
  [ 6] java.util.jar.JarFile.getManifestFromReference
  [ 7] java.util.jar.JarFile.getManifest
  [ 8] jdk.internal.loader.URLClassPath$JarLoader$2.getManifest
  [ 9] jdk.internal.loader.BuiltinClassLoader.defineClass
  [10] jdk.internal.loader.BuiltinClassLoader.findClassOnClassPathOrNull
  [11] jdk.internal.loader.BuiltinClassLoader.loadClassOrNull
  [12] jdk.internal.loader.BuiltinClassLoader.loadClass
  [13] jdk.internal.loader.ClassLoaders$AppClassLoader.loadClass
  [14] java.lang.ClassLoader.loadClass
  [15] java.lang.Class.forName0
  [16] java.lang.Class.forName
  [17] sun.launcher.LauncherHelper.loadMainClass
  [18] sun.launcher.LauncherHelper.checkAndLoadMain
  ```

- `jfr` - profile format used by the JDK Flight Recorder. The `jfr` format collects data
  about the JVM as well as the Java application running on it. async-profiler can generate output in `jfr` format
  compatible with tools capable of viewing and analyzing `jfr` files. JDK Mission Control (JMC) and Intellij IDEA are
  some of many options to visualize `jfr` files. More details [here](JfrVisualization.md).
