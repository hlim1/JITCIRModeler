// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt16Array

function uint16array(x) {
    let uint16 = new Uint16Array(x);
    uint16[0] = 41;
}

for (let i = 0; i < 10000; i++) {
    uint16array([21,31]);
}
