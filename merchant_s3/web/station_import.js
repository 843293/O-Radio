const StationImport = (() => {
  const bytes = s => new TextEncoder().encode(s).length;
  function validate(s) {
    if (!s || typeof s !== 'object' || Array.isArray(s) || typeof s.name !== 'string' || typeof s.url !== 'string') throw Error('电台必须包含 name 和 url');
    const name = s.name.trim(), url = s.url.trim(), group = typeof s.group === 'string' ? s.group.trim() : '';
    if (!name || bytes(name) > 96 || /[\x00-\x1f]/.test(name) || bytes(group) > 64) throw Error('名称最多 96 字节、分组最多 64 字节');
    if (url.length > 768 || /[^\x21-\x7e]/.test(url) || !/^https?:\/\/[^/?#\s]+(?:[/?#]|$)/.test(url)) throw Error('需要 HTTP/HTTPS 音频流地址，空格和中文请先 URL 编码');
    return {name,url,group};
  }
  function parse(text, format='auto') {
    if (typeof text !== 'string' || bytes(text) > 120000) throw Error('导入文件不能超过 120 KB');
    text = text.replace(/^\uFEFF/, '').trim();
    if (!text) throw Error('列表为空');
    let items=[];
    if (format==='json' || (format==='auto' && /^[\[{]/.test(text) && !/^\[playlist\]/i.test(text))) {
      const data=JSON.parse(text);items=Array.isArray(data)?data:data.stations;
      if(!Array.isArray(items))throw Error('JSON 必须是数组，或含 stations 数组');
    } else if(format==='pls'||/^\[playlist\]/i.test(text)) {
      const map=new Map();
      for(const line of text.split(/\r?\n/)){
        const m=line.match(/^\s*(File|Title)(\d+)\s*=\s*(.*?)\s*$/i);if(!m)continue;
        const n=Number(m[2]);if(n<1||n>10000)throw Error('PLS 序号不正确');
        if(!map.has(n))map.set(n,{});map.get(n)[m[1].toLowerCase()]=m[3];
      }
      items=[...map].sort((a,b)=>a[0]-b[0]).filter(([,v])=>v.file).map(([i,v])=>({name:v.title||`电台 ${i}`,url:v.file}));
    } else {
      if(/#EXT-X-/i.test(text))throw Error('这是 HLS 分片清单，请直接添加该 .m3u8 的网络地址，不要导入分片');
      let name='',group='';
      for(const raw of text.split(/\r?\n/)){
        const line=raw.trim();if(!line)continue;
        if(/^#EXTINF:/i.test(line)){
          const m=line.match(/group-title="([^"]*)"/i);group=m?m[1]:'';
          // EXTINF attribute values may themselves contain commas.
          let quoted=false,split=-1;for(let i=0;i<line.length;i++){if(line[i]==='"')quoted=!quoted;if(line[i]===','&&!quoted){split=i;break;}}
          name=split>=0?line.slice(split+1).trim():'';continue;
        }
        if(line.startsWith('#'))continue;
        items.push({name:name||`电台 ${items.length+1}`,url:line,group});name='';group='';
      }
    }
    if(!items.length)throw Error('没有找到电台地址');
    if(items.length>100)throw Error('最多导入 100 个电台');
    const seen=new Set();return items.map(validate).filter(s=>{if(seen.has(s.url))return false;seen.add(s.url);return true});
  }
  return {validate,parse};
})();
