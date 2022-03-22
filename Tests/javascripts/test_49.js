// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/search

const paragraph = 'The quick brown fox jumps over the lazy dog. If the dog barked, was it really lazy?';

// any character that is not a word character or whitespace
const regex = /[^\w\s]/g;

let result;

function search(str) {
    return str.search(regex);
}

for (let i = 0; i < 10000; i++) {
    result = search(paragraph);
}
