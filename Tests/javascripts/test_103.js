// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt8Array

function uint8array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new Uint8Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    uint8array(8);
}
