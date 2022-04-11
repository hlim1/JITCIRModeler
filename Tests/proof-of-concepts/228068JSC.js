function f() {
      v0 = (-0.0).toLocaleString();
      return parseInt(v0);
}
noInline(f);

// Interpreter Only.
let a0 = f();
print(1 / a0);

// Interpreter + JIT Compiler.
// Expected: -Infinity
// Actual:   Infinity
for (let i = 0; i < 10000; i++) { f(); }
let a3 = f();
print(1 / a3);
