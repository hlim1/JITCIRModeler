// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int32Array

function int32array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new Int32Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    int32array(16);
}
