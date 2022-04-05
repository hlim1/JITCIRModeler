// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int32Array

function int32array(x) {
    let y = new Int32Array(x);
    let z = new Int32Array(y);
    z[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    int32array([21,31]);
}
