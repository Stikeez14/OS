#!/bin/bash

check_for_non_ascii() {
    if [[ $(tr -d '[:print:]' < "$1") ]]; then
        echo "(Syntactic Analysis) \"$(basename "$1")\" contains non-ASCII characters."
        exit 1
    fi
}

check_for_keywords() {
    local keywords=("corrupted" "dangerous" "risk" "attack" "malware" "malicious")
    for keyword in "${keywords[@]}"; do
        if grep -q -i "$keyword" "$1"; then
            echo "(Syntactic Analysis) \"$(basename "$1")\" contains keyword: $keyword"
            exit 1
        fi
    done
}

check_file_stats() {
    local lines=$(wc -l < "$1")
    local words=$(wc -w < "$1")
    local characters=$(wc -c < "$1")

    echo "(Syntactic Analysis) \"$(basename "$1")\" has: $lines lines, $words words, $characters characters"
}

main() {
    if [ $# -ne 1 ]; then
        echo "Usage: $0 <filename>"
        exit 1
    fi

    filename="$1"

    check_for_non_ascii "$filename"
    check_for_keywords "$filename"
    check_file_stats "$filename"
}

main "$@"
