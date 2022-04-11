function lessThan()
{
    return 0n < 2n;
}
noInline(lessThan);

for (var i = 0; i < 30000; ++i) {
    lessThan();
}
