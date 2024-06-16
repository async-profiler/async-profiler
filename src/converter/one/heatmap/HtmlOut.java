package one.heatmap;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

public class HtmlOut {

    private final OutputStream out;

    private int pos = 0;

    public HtmlOut(OutputStream out) {
        this.out = out;
    }

    public void resetPos() {
        pos = 0;
    }

    public void nextByte(int ch) throws IOException {
        int c = ch;
        switch (ch) {
            case 0:
                c = 127;
                break;
            case '\r':
                c = 126;
                break;
            case '&':
                c = 125;
                break;
            case '<':
                c = 124;
                break;
            case '>':
                c = 123;
                break;
        }
        out.write(c);
        pos++;
    }

    public void writeVar(long v) throws IOException {
        while (v >= 61) {
            int b = 61 + (int) (v % 61);
            nextByte(b);
            v /= 61;
        }
        nextByte((int) v);
    }

    public void write6(int v) throws IOException {
        if ((v & (0xFFFFFFC0)) != 0) {
            throw new IllegalArgumentException("Value " + v + " is out of bounds");
        }
        nextByte(v);
    }

    public void write18(int v) throws IOException {
        if ((v & (~0x3FFFF)) != 0) {
            throw new IllegalArgumentException("Value " + v + " is out of bounds");
        }
        for (int i = 0; i < 3; i++) {
            nextByte(v & 0x3F);
            v >>>= 6;
        }
    }

    public void write30(int v) throws IOException {
        if ((v & 0xFFFF_FFFF_C000_0000L) != 0) {
            throw new IllegalArgumentException("Value " + v + " is out of bounds");
        }
        for (int i = 0; i < 5; i++) {
            nextByte(v & 0x3F);
            v >>>= 6;
        }
    }

    public void write(byte[] data) throws IOException {
        out.write(data);
        pos += data.length;
    }

    public int pos() {
        return pos;
    }

    public PrintStream asPrintableStream() {
        return new PrintStream(out);
    }

}
