int b(){
    return 10;
}

int d(){
    return b();
}
int a(){
    int c = b();

    if(c)
        a();
    else
        d();

    return c;
}