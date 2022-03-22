// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/match

const paragraph = 'The quick brown fox jumps over the lazy dog. It barked.';
const regex = /[A-Z]/g;
let result;

function match(str) {
    return str.match(regex);
}

for (let i = 0; i < 10000; i++) {
    result = match(paragraph);
}
