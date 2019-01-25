# diffxml
produce xml from a diff output (only fragment, place surrounding tag yourself)

It is very crude and missing a lot of basic error handling!!

In doubt adjust the BUFSIZE upward.

Currently only supports Unix-style text only. (i.e. only newline as line terminator)

compile: `gcc -Wall -o diffxml diffxml.c`

<pre>echo '&lt;diffs&gt;' &gt; diff.xml
diff -N -w -r -p dira/ dirb/ | diffxml &gt;&gt; diff.xml
echo '&lt;/diffs&gt;' &gt;&gt; diff.xml`
</pre>

for example linux kernels, breaking it down across the toplevel directory structure
to make the chunks smaller and the output better structured: 

<pre>echo '&lt;knldiffs&gt;' &gt; knldiffs.xml
for f in $( for d in linux-4.14.1/* linux-4.14.2/*
do
   echo ${d##*/}
done |sort -u )
do
   echo '&lt;blk n="'$f'"&gt;' &gt;&gt; knldiffs.xml
   diff  -N -w -r -p linux-4.14.1/$f linux-4.14.2/$f | diffxml &gt;&gt; knldiffs.xml
   echo '&lt;/blk&gt;' &gt;&gt; knldiffs.xml
done
echo '&lt;/knldiffs&gt;' &gt;&gt; knldiffs.xml</pre>
