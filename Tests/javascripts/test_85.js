// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int32Array

function int32array(x) {
    let int32 = new Int32Array(x);
    int32[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    int32array(2);
}
