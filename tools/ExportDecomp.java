// Ghidra headless script: decompile every function into one C file + strings list
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;
import java.io.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        PrintWriter pw = new PrintWriter(new FileWriter(out + "/farm.c"));
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int n = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            DecompileResults r = ifc.decompileFunction(f, 60, monitor);
            pw.println("// ==== " + f.getName() + " @ " + f.getEntryPoint());
            if (r != null && r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC());
            else pw.println("// decompile failed");
            n++;
        }
        pw.close();
        PrintWriter sw = new PrintWriter(new FileWriter(out + "/strings.txt"));
        DataIterator di = currentProgram.getListing().getDefinedData(true);
        while (di.hasNext()) {
            Data d = di.next();
            if (d.hasStringValue()) sw.println(d.getAddress() + "\t" + d.getDefaultValueRepresentation());
        }
        sw.close();
        println("Exported " + n + " functions");
    }
}
