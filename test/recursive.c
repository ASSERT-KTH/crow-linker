int a(int b){

    if(b)
        return a(b - 1);

    return b;
}