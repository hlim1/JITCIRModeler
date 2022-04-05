// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt16Array

function uint16array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new Uint16Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    uint16array(8);
}
