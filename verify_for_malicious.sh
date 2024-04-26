#!/bin/bash

check_file_stats() {
    local lines=$(wc -l < "$1")
    local words=$(wc -w < "$1")
    local characters=$(wc -c < "$1")

    if [[ $lines -lt 3 && $words -gt 999 && $characters -gt 1999 ]]; then
        echo "(Syntactic Analysis) \"$(basename "$1")\" is suspicious after analyzing the number of lines, words, and characters."
        return 1
    else 
        return 0
    fi
}

check_for_non_ascii() {
    if [[ $(tr -d '[:print:]' < "$1") ]]; then
        echo "(Syntactic Analysis) \"$(basename "$1")\" contains non-ASCII characters."
        return 1
    fi
}

check_for_keywords() {
    local keywords=("corrupted" "dangerous" "risk" "attack" "malware" "malicious")
    for keyword in "${keywords[@]}"; do
        if grep -q -i "$keyword" "$1"; then
            echo "(Syntactic Analysis) \"$(basename "$1")\" contains keyword: $keyword"
            return 1
        fi
    done
}

main() {
    if [ $# -ne 1 ]; then
        echo "Usage: $0 <filename>"
        exit 1
    fi

    filename="$1"

    check_file_stats "$filename"
    if [[ $? -eq 1 ]]; then
        check_for_non_ascii "$filename"
        check_for_keywords "$filename"
    fi
}

main "$@"
