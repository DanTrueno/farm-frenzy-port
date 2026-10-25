// Decompile (creating functions if needed) at given addresses
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
public class DecompAt extends GhidraScript {
    public void run() throws Exception {
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        for (String a : getScriptArgs()) {
            Address ad = toAddr(a);
            Function f = getFunctionAt(ad);
            if (f == null) { disassemble(ad); f = createFunction(ad, null); }
            if (f == null) { println("no function at " + a); continue; }
            DecompileResults r = ifc.decompileFunction(f, 60, monitor);
            println("==== " + a + "\n" + r.getDecompiledFunction().getC());
        }
    }
}
