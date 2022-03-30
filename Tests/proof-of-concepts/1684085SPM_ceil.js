with ({}) {}

function ceil(a, b) {
        return Math.ceil(a / b) | 0;
}

for (var i = 0; i < 50; i++) {
        ceil(5,5);
}

// Expected Output: 2
print(ceil(5,3));
