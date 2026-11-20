"""
gui.py - Tkinter Graphical User Interface for Text Compression Tool using Huffman Coding
Strictly satisfies College Specification: Objective 8 ("CLI or Tkinter GUI")

Features:
1. File selection via native file explorer dialog
2. Compression (Huffman, RLE, Auto) into .huff / .huffz
3. Decompression with bit-for-bit restoration
4. Direct Data Integrity Verification (Objective 10)
5. Multi-method performance benchmarking (Objective 5 & 7)
6. Real-time character frequency analysis and prefix-free code table view
"""

import os
import sys
import time
import subprocess
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

EXE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "huffz.exe")

class HuffmanCompressionGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Text Compression Tool using Huffman Coding (DSA Project)")
        self.root.geometry("860x680")
        self.root.minsize(780, 600)

        # Style configuration
        self.style = ttk.Style()
        self.style.theme_use("clam")

        self.input_file = tk.StringVar()
        self.mode_var = tk.StringVar(value="huffman")

        self.build_ui()

    def build_ui(self):
        # Header banner
        header = tk.Frame(self.root, bg="#1e293b", pady=14)
        header.pack(fill="x")

        title_lbl = tk.Label(
            header,
            text="Text Compression Tool using Huffman Coding",
            font=("Segoe UI", 16, "bold"),
            fg="#38bdf8",
            bg="#1e293b"
        )
        title_lbl.pack()

        sub_lbl = tk.Label(
            header,
            text="Min-Priority Queue | Binary Prefix Tree | RLE | Bit-level File Handling",
            font=("Segoe UI", 10),
            fg="#94a3b8",
            bg="#1e293b"
        )
        sub_lbl.pack()

        # Main container notebook
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=16, pady=12)

        # Tab 1: Compression & Decompression
        self.tab_main = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.tab_main, text="  Compress & Decompress  ")

        # Tab 2: Performance Benchmark
        self.tab_bench = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.tab_bench, text="  Benchmark & Comparison  ")

        # Tab 3: Data Integrity Verification
        self.tab_verify = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.tab_verify, text="  Verify Data Integrity  ")

        self.setup_main_tab()
        self.setup_bench_tab()
        self.setup_verify_tab()

    def setup_main_tab(self):
        # File selector frame
        file_frame = ttk.LabelFrame(self.tab_main, text="1. Select Input File", padding=10)
        file_frame.pack(fill="x", pady=6)

        entry = ttk.Entry(file_frame, textvariable=self.input_file, font=("Segoe UI", 10))
        entry.pack(side="left", fill="x", expand=True, padx=(0, 8))

        browse_btn = ttk.Button(file_frame, text="Browse...", command=self.browse_input)
        browse_btn.pack(side="right")

        # Mode frame
        mode_frame = ttk.LabelFrame(self.tab_main, text="2. Compression Algorithm", padding=10)
        mode_frame.pack(fill="x", pady=6)

        ttk.Radiobutton(mode_frame, text="Huffman Coding (Min-Priority Queue)", variable=self.mode_var, value="huffman").pack(side="left", padx=12)
        ttk.Radiobutton(mode_frame, text="Run-Length Encoding (RLE)", variable=self.mode_var, value="rle").pack(side="left", padx=12)
        ttk.Radiobutton(mode_frame, text="Auto Mode (Selects Smallest Output)", variable=self.mode_var, value="auto").pack(side="left", padx=12)

        # Action Buttons
        btn_frame = ttk.Frame(self.tab_main, padding=8)
        btn_frame.pack(fill="x")

        comp_btn = tk.Button(btn_frame, text="Compress to .huff", font=("Segoe UI", 10, "bold"), bg="#0284c7", fg="white", padx=14, pady=6, relief="flat", command=self.run_compress)
        comp_btn.pack(side="left", padx=6)

        decomp_btn = tk.Button(btn_frame, text="Decompress to .txt", font=("Segoe UI", 10, "bold"), bg="#10b981", fg="white", padx=14, pady=6, relief="flat", command=self.run_decompress)
        decomp_btn.pack(side="left", padx=6)

        trace_btn = tk.Button(btn_frame, text="View DSA Tree Trace", font=("Segoe UI", 10), bg="#475569", fg="white", padx=14, pady=6, relief="flat", command=self.run_demo)
        trace_btn.pack(side="left", padx=6)

        # Output / Log console
        log_frame = ttk.LabelFrame(self.tab_main, text="Execution Trace & Results", padding=8)
        log_frame.pack(fill="both", expand=True, pady=6)

        self.log_txt = tk.Text(log_frame, bg="#0f172a", fg="#38bdf8", font=("Consolas", 10), wrap="word")
        self.log_txt.pack(fill="both", expand=True)

    def setup_bench_tab(self):
        desc = ttk.Label(self.tab_bench, text="Objective 5 & 7: Compare Huffman vs RLE across Natural Language, Repetitive Text, and Source Code", font=("Segoe UI", 10, "italic"))
        desc.pack(anchor="w", pady=4)

        btn = tk.Button(self.tab_bench, text="Run Full Benchmark Suite", font=("Segoe UI", 10, "bold"), bg="#6366f1", fg="white", padx=16, pady=6, relief="flat", command=self.run_bench)
        btn.pack(anchor="w", pady=6)

        self.bench_txt = tk.Text(self.tab_bench, bg="#0f172a", fg="#4ade80", font=("Consolas", 10), wrap="word")
        self.bench_txt.pack(fill="both", expand=True, pady=6)

    def setup_verify_tab(self):
        desc = ttk.Label(self.tab_verify, text="Objective 10: Verify Data Integrity (Bit-for-Bit match and CRC-32 Checksum)", font=("Segoe UI", 10, "italic"))
        desc.pack(anchor="w", pady=4)

        f_frame = ttk.Frame(self.tab_verify)
        f_frame.pack(fill="x", pady=6)

        self.f1_var = tk.StringVar()
        self.f2_var = tk.StringVar()

        ttk.Label(f_frame, text="Original File:").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(f_frame, textvariable=self.f1_var, width=50).grid(row=0, column=1, padx=6)
        ttk.Button(f_frame, text="Browse...", command=lambda: self.browse_any(self.f1_var)).grid(row=0, column=2)

        ttk.Label(f_frame, text="Decompressed:").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Entry(f_frame, textvariable=self.f2_var, width=50).grid(row=1, column=1, padx=6)
        ttk.Button(f_frame, text="Browse...", command=lambda: self.browse_any(self.f2_var)).grid(row=1, column=2)

        v_btn = tk.Button(self.tab_verify, text="Verify Lossless Equality", font=("Segoe UI", 10, "bold"), bg="#059669", fg="white", padx=14, pady=6, relief="flat", command=self.run_verify)
        v_btn.pack(anchor="w", pady=8)

        self.verify_txt = tk.Text(self.tab_verify, bg="#0f172a", fg="#facc15", font=("Consolas", 10), wrap="word")
        self.verify_txt.pack(fill="both", expand=True, pady=6)

    def browse_input(self):
        f = filedialog.askopenfilename(title="Select File to Compress or Decompress")
        if f:
            self.input_file.set(f)

    def browse_any(self, target_var):
        f = filedialog.askopenfilename(title="Select File")
        if f:
            target_var.set(f)

    def execute_cli(self, args):
        if not os.path.exists(EXE_PATH):
            return f"Error: huffz.exe binary not found at {EXE_PATH}.\nPlease build the project using 'make' or g++."
        try:
            cmd = [EXE_PATH] + args
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=os.path.dirname(EXE_PATH))
            return res.stdout + ("\n" + res.stderr if res.stderr else "")
        except Exception as e:
            return f"Execution failed: {str(e)}"

    def run_compress(self):
        f = self.input_file.get()
        if not f or not os.path.exists(f):
            messagebox.showerror("Error", "Please select a valid input file first.")
            return

        out = f + ".huff"
        mode = self.mode_var.get()
        self.log_txt.delete("1.0", "end")
        self.log_txt.insert("end", f"Compressing '{f}' using mode '{mode}'...\n")
        self.root.update_idletasks()

        res = self.execute_cli(["c", f, out, "--mode", mode])
        self.log_txt.insert("end", res)
        self.log_txt.insert("end", f"\nSaved compressed file to: {out}\n")

    def run_decompress(self):
        f = self.input_file.get()
        if not f or not os.path.exists(f):
            messagebox.showerror("Error", "Please select a compressed file first.")
            return

        out = f.replace(".huff", "").replace(".huffz", "") + "_restored.txt"
        self.log_txt.delete("1.0", "end")
        self.log_txt.insert("end", f"Decompressing '{f}'...\n")
        self.root.update_idletasks()

        res = self.execute_cli(["d", f, out])
        self.log_txt.insert("end", res)
        self.log_txt.insert("end", f"\nRestored original file to: {out}\n")

    def run_demo(self):
        f = self.input_file.get()
        if not f or not os.path.exists(f):
            sample = os.path.join(os.path.dirname(os.path.abspath(__file__)), "samples", "natural_english.txt")
            f = sample

        self.log_txt.delete("1.0", "end")
        self.log_txt.insert("end", f"Running DSA Step-by-Step Execution Trace on: {f}...\n")
        self.root.update_idletasks()

        res = self.execute_cli(["demo", f])
        self.log_txt.insert("end", res)

    def run_bench(self):
        self.bench_txt.delete("1.0", "end")
        self.bench_txt.insert("end", "Running benchmark across sample files...\n")
        self.root.update_idletasks()

        samples_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "samples")
        files = [
            os.path.join(samples_dir, "natural_english.txt"),
            os.path.join(samples_dir, "repetitive.txt"),
            os.path.join(samples_dir, "source_code.cpp"),
            os.path.join(samples_dir, "log_like.txt")
        ]
        res = self.execute_cli(["bench"] + files)
        self.bench_txt.insert("end", res)

    def run_verify(self):
        f1 = self.f1_var.get()
        f2 = self.f2_var.get()
        if not f1 or not f2 or not os.path.exists(f1) or not os.path.exists(f2):
            messagebox.showerror("Error", "Please select both original and decompressed files.")
            return

        self.verify_txt.delete("1.0", "end")
        self.verify_txt.insert("end", "Performing bit-for-bit lossless integrity verification...\n")
        self.root.update_idletasks()

        res = self.execute_cli(["verify", f1, f2])
        self.verify_txt.insert("end", res)

def main():
    root = tk.Tk()
    app = HuffmanCompressionGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
