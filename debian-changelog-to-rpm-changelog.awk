#!/bin/gawk -f

BEGIN {
    RS="(^|\n)tuxedo-(keyboard-ite|keyboard|drivers) "
    FS="\n"
    print "%changelog"
}

NR>1 {
    split($1, a, " ")
    split($(NF-1), b, ">")
    split(b[2], c, " ")
    gsub(/,/, "", c[1])
    gsub(/ -- /, "", b[1])
    gsub(/\(/, "", a[1])
    gsub(/\)/, "", a[1])
    print "* " c[1] " " c[3] " " c[2] " " c[4] " " b[1] " " a[1] "-1"
    for (i=3; i<NF-2; ++i) {
        gsub(/  \*/, "-", $i)
        gsub(/    /, "  ", $i)
        print $i
    }
}
