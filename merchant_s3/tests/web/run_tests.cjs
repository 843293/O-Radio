const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../..');
const parser = fs.readFileSync(path.join(root,'web/station_import.js'),'utf8');
const tests = fs.readFileSync(path.join(__dirname,'importer_tests.js'),'utf8');
const html = fs.readFileSync(path.join(root,'web/index.html'),'utf8').replace('/* IMPORTER */',parser);
new Function(html.split('<script>')[1].split('</script>')[0]);
const result = new Function('TextEncoder',parser+'\n'+tests)(TextEncoder);
console.log('PASS: '+result.passed+' importer tests; page JS syntax valid');
