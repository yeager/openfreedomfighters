// Exports private analysis metadata and a Ghidra instruction listing.
// The output is a research artifact and must never be committed or distributed.
//@category OpenFreedomFighters

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;

public class ExportPrivateAnalysis extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 1) {
            throw new IllegalArgumentException("Expected one private output directory argument");
        }
        File outputDirectory = new File(arguments[0]);
        if (!outputDirectory.isDirectory() && !outputDirectory.mkdirs()) {
            throw new IllegalStateException("Could not create private output directory");
        }

        Listing listing = currentProgram.getListing();
        long instructionCount = 0;
        long instructionBytes = 0;
        try (BufferedWriter output = new BufferedWriter(
                new FileWriter(new File(outputDirectory, "ghidra-disassembly.txt")))) {
            output.write("; PRIVATE CLEAN-ROOM RESEARCH ARTIFACT - DO NOT COMMIT OR DISTRIBUTE\n");
            output.write("; input_sha256 " + currentProgram.getExecutableSHA256() + "\n");
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isExecute()) {
                    continue;
                }
                output.write("\n; block " + block.getName() + " " + block.getStart() + "-" + block.getEnd() + "\n");
                InstructionIterator instructions = listing.getInstructions(
                    new AddressSet(block.getStart(), block.getEnd()), true);
                while (instructions.hasNext()) {
                    monitor.checkCancelled();
                    Instruction instruction = instructions.next();
                    Function function = currentProgram.getFunctionManager().getFunctionAt(instruction.getAddress());
                    if (function != null) {
                        output.write("\n; function " + function.getName() + "\n");
                    }
                    output.write(instruction.getAddress().toString());
                    output.write("  ");
                    output.write(instruction.toString());
                    output.write("\n");
                    instructionCount++;
                    instructionBytes += instruction.getLength();
                }
            }
        }

        long functionCount = 0;
        try (BufferedWriter output = new BufferedWriter(
                new FileWriter(new File(outputDirectory, "ghidra-functions.tsv")))) {
            output.write("entry\tname\tsize\n");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                Function function = functions.next();
                output.write(function.getEntryPoint().toString());
                output.write("\t");
                output.write(function.getName().replace('\t', '_'));
                output.write("\t");
                output.write(Long.toString(function.getBody().getNumAddresses()));
                output.write("\n");
                functionCount++;
            }
        }

        try (BufferedWriter output = new BufferedWriter(
                new FileWriter(new File(outputDirectory, "ghidra-summary.txt")))) {
            output.write("input_sha256=" + currentProgram.getExecutableSHA256() + "\n");
            output.write("language=" + currentProgram.getLanguageID() + "\n");
            output.write("compiler=" + currentProgram.getCompilerSpec().getCompilerSpecID() + "\n");
            output.write("instructions=" + instructionCount + "\n");
            output.write("instruction_bytes=" + instructionBytes + "\n");
            output.write("functions=" + functionCount + "\n");
        }
        println("Private analysis exported to " + outputDirectory.getAbsolutePath());
    }
}
