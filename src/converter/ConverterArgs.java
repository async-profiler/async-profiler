public class ConverterArgs {
    public String title = "Flame Graph";
    public boolean reverse;
    public double minwidth;
    public int skip;
    public String input;
    public String output;
    public long t1 = Long.MIN_VALUE;
    public long t2 = Long.MAX_VALUE;
    public boolean collapsed = false;

    ConverterArgs(String... args) {
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (!arg.startsWith("--") && !arg.isEmpty()) {
                if (input == null) {
                    input = arg;
                } else {
                    output = arg;
                }
            } else if (arg.equals("--title")) {
                title = args[++i];
            } else if (arg.equals("--reverse")) {
                reverse = true;
            } else if (arg.equals("--minwidth")) {
                minwidth = Double.parseDouble(args[++i]);
            } else if (arg.equals("--skip")) {
                skip = Integer.parseInt(args[++i]);
            } else if (arg.equals("--t1")) {
                t1 = Long.parseLong(args[++i]);
            } else if (arg.equals("--t2")) {
                t2 = Long.parseLong(args[++i]);
            } else if (arg.equals("--collapsed")) {
                collapsed = true;
            }
        }
    }
}