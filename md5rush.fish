#!/usr/bin/fish
for i in (seq 1 32)
    echo "Seaching for $i-treasure..."
    ./md5rush -z $i -p treasure-(math $i - 1) -o treasure-$i
    echo
end
