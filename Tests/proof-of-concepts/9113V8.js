// Set field representation to float64.
let dummy = { x : 0.1 };

// Create object with const field 0.
let o = { x : 0 };

function f(o, v) {
  o.x = v;
}

f(o, 0);
f(o, 0);
print (1 / o.x);

%OptimizeFunctionOnNextCall(f);
f(o, -0);

// This fails because Turbofan's property store is guarded 
// by NumberEqual, and NumberEqual(0, -0) is true.
print (1 / o.x);
