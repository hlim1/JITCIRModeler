// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/replace

const p = 'The quick brown fox jumps over the lazy dog. If the dog reacted, was it really lazy?';
let result;

function replace(str1, target, to) {
    return str1.replace(target, to);
}

for (let i = 0; i < 10000; i++) {
    result = replace(p, "dog", "monkey");
}
