#!/usr/bin/bash

get-mirror-description() {
        mirror="$1"
        sed -n '4s/^# //p' "$mirror"
}

get-mirror-main-url() {
	mirror="$1"
	source "$mirror"
	echo "$MAIN"
}

get-mirror-root-url() {
	mirror="$1"
	source "$mirror"
	echo "$ROOT"
}

get-mirror-x11-url() {
	mirror="$1"
	source "$mirror"
	echo "$X11"
}

generate-mirror-table() {
	mirror="$1"

	main_entry='`deb '"$(get-mirror-main-url $mirror)"' stable main`'
	root_entry='`deb '"$(get-mirror-root-url $mirror)"' root stable`'
	x11_entry='`deb '"$(get-mirror-x11-url $mirror)"' x11 main`'

	# Calculate length of horizontal sep in sources.list entry column, to get a pretty table
	len="${#main_entry}"

	printf "| Repository                                             | %-${len}s |\n" "sources.list entry"
	printf "|:-------------------------------------------------------|:%-${len}s-|\n" | tr ' ' '-'
	printf "| [Main](https://github.com/termux/termux-packages)      | %-${len}s |\n" "$main_entry"
	printf "| [Root](https://github.com/termux/termux-root-packages) | %-${len}s |\n" "$root_entry"
	printf "| [X11](https://github.com/termux/x11-packages)          | %-${len}s |\n" "$x11_entry"
}

orig_dir="$(pwd)"
cd "$(realpath "$(dirname "$0")")"

: "${TMPDIR:=/tmp}"
export TMPDIR

mirror_tmpfile="$(mktemp $TMPDIR/Mirrors.md.XXXXX)"
cat ../wiki/mirrors_header.md > "$mirror_tmpfile"

for group in */; do
	group_name="$(basename $group)"
	group_name="${group_name^}"

	echo "" >> "$mirror_tmpfile"
	echo "#### Mirrors in ${group_name/_/ }" >> "$mirror_tmpfile"
	echo "" >> "$mirror_tmpfile"

	for mirror in $(git ls-files "$group"); do
		if head -n 4 "$mirror" | grep -qv '^#'; then
			echo "Error: $mirror does not have 4 header lines starting with #" > /dev/stderr
			exit 1
		fi

                # Split 3rd line of mirror on "|" to get owner and url
		IFS='|' read mirror_owner mirror_url <<<$(sed -n '3s/^# //p' "$mirror");
		echo "##### Mirror by [$(echo ${mirror_owner}|xargs)]($(echo ${mirror_url}|xargs))" >> "$mirror_tmpfile"
		echo "" >> "$mirror_tmpfile"
		get-mirror-description "$mirror" >> "$mirror_tmpfile"
		echo "" >> "$mirror_tmpfile"
		generate-mirror-table "$mirror" >> "$mirror_tmpfile"
		echo "" >> "$mirror_tmpfile"
	done
done

mv "$mirror_tmpfile" "${orig_dir}"/Mirrors.md
echo "Mirrors.md generation done"
