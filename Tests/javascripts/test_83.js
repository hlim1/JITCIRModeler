// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int16Array

function int16array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new Int16Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    int16array(8);
}
