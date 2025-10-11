#!/usr/bin/awk -f

BEGIN {
    print "package gitin"
    print "import \"github.com/friedelschoen/gitin-go/preview\""
    print "var Filetypes = []preview.FileType{"
}
/^#/ {
    next
}
$0 {
    printf("{ \"%s\", \"%s\", \"%s\" },\n", $1, $2, $3)
}
END { print "}" }
