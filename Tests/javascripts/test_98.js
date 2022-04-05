// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt32Array

function uint32array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new Uint32Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    uint32array(8);
}
