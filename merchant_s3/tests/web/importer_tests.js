let passed=0;
function check(ok,label){if(!ok)throw Error('FAIL: '+label);passed++}
function rejects(text,label){try{StationImport.parse(text)}catch(e){passed++;return}throw Error('FAIL: '+label)}
let a=StationImport.parse('\uFEFF#EXTM3U\r\n#EXTINF:-1 group-title="Music, Jazz",乡间爵士\r\nhttps://radio.example/live.mp3\r\n');
check(a.length===1&&a[0].name==='乡间爵士'&&a[0].group==='Music, Jazz','UTF8 BOM / CRLF / quoted comma');
a=StationImport.parse('[playlist]\nTitle2=第二台\nFile2=https://radio.example/b\nFile1=http://radio.example/a\nTitle1=第一台\nNumberOfEntries=2');
check(a.length===2&&a[0].name==='第一台'&&a[1].name==='第二台','PLS indexed ordering');
a=StationImport.parse(JSON.stringify({stations:[{id:100,name:'Test',url:'http://a.example/x',group:'g'}]}));
check(a[0].id===undefined&&a[0].group==='g','JSON envelope / strip imported IDs');
a=StationImport.parse('http://a.example/x\nhttp://a.example/x\nhttps://b.example/y');
check(a.length===2,'duplicate URL removal');
rejects('#EXTM3U\n#EXT-X-TARGETDURATION:10\nchunk.ts','reject HLS fragments');
rejects('file:///tmp/song.mp3','reject non HTTP protocol');
rejects('http://a.example/a b','reject URL whitespace');
rejects('http://a.example/中文','reject raw non-ASCII URL');
rejects('http:///bad','reject empty host');
rejects(JSON.stringify([{name:'x'.repeat(97),url:'https://a.example'}]),'UTF8 byte length');
rejects(JSON.stringify([{name:'x',url:'http://a.example/'+'x'.repeat(768)}]),'URL size');
rejects('[]','empty array');
rejects('not a URL','invalid plaintext');
rejects(JSON.stringify(Array.from({length:101},(_,i)=>({name:'a',url:`https://a.example/${i}`}))),'station count');
return {passed};
