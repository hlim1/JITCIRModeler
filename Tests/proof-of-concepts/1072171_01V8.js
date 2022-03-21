function crash() { 
    for (a=0;a<2;a++)
        for (let i = -0.0; i < 1000; i++) { 
            confused = Math.max(-1, i);
        } 
    confused[0];
}

crash();
%OptimizeFunctionOnNextCall(crash);
crash();
