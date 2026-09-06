import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Workbook, SpreadsheetFile } from '@oai/artifact-tool';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const data=JSON.parse(await fs.readFile(path.join(root,'hardware/bom-data.json'),'utf8'));
const wb=Workbook.create();
const purchasing=wb.worksheets.add('Purchasing');
const assembly=wb.worksheets.add('PCB assembly');
const date=new Date(data.date+'T12:00:00Z');
const pheaders=['ID','Qty','Part','MPN or specification','Unit USD','Line USD','Price basis','Installation','Notes','Source URL','Date'];
const aheaders=['References','Qty','Part','Manufacturer','MPN','Package','Unit USD','Line USD','Price basis','Installation','Assembly notes','Source URL','Date'];
const prows=data.purchasing.map(r=>{if(r.length!==9)throw Error('Purchasing row width '+r[0]);return [...r.slice(0,5),null,...r.slice(5),date]});
const arows=data.assembly.map(r=>{if(r.length!==11)throw Error('Assembly row width '+r[0]);return [...r.slice(0,7),null,...r.slice(7),date]});
function setup(s,headers,rows,price,line,last,title,note,widths){
 s.showGridLines=false;
 s.getRange(`A1:${last}${rows.length+4}`).format.font={name:'Helvetica',size:11,color:'#20262D'};
 s.getRange('A1').values=[[title]];s.getRange('A1').format.font={name:'Helvetica',size:15,bold:true};
 s.getRange('A2').values=[[note]];s.getRange('A2').format.font={name:'Helvetica',italic:true,size:11,color:'#53606C'};
 s.getRange(`A4:${last}4`).values=[headers];
 s.getRange(`A5:${last}${rows.length+4}`).values=rows;
 s.getRange(`A4:${last}4`).format={fill:'#374554',font:{name:'Helvetica',bold:true,color:'#FFFFFF'},rowHeight:32,wrapText:true};
 s.getRange(`A5:${last}${rows.length+4}`).format.wrapText=true;
 s.getRange(`A5:${last}${rows.length+4}`).format.rowHeight=60;
 s.getRange(`A5:${last}${rows.length+4}`).format.verticalAlignment='top';
 s.getRange(`B5:B${rows.length+4}`).setNumberFormat('0');
 for(let i=0;i<rows.length;i++)if(!Number.isInteger(rows[i][1]))s.getRange(`B${i+5}`).setNumberFormat('0.00');
 s.getRange(`${price}5:${line}${rows.length+4}`).setNumberFormat('"$"0.00');
 s.getRange(`${last}5:${last}${rows.length+4}`).setNumberFormat('yyyy-mm-dd');
 for(let i=0;i<widths.length;i++)s.getRange(`${String.fromCharCode(65+i)}1:${String.fromCharCode(65+i)}${rows.length+4}`).format.columnWidth=widths[i];
 s.getRange(`${line}5`).formulas=[[`=IF(${price}5="","",B5*${price}5)`]];
 s.getRange(`${line}5:${line}${rows.length+4}`).fillDown();
 s.freezePanes.freezeRows(4);s.freezePanes.freezeColumns(2);
 const table=s.tables.add(`A4:${last}${rows.length+4}`,true,s===purchasing?'PurchasingParts':'AssemblyParts');table.style='TableStyleLight9';
}
setup(purchasing,pheaders,prows,'E','F','K','AirMon purchasing BOM','USD, '+data.date+'. Blank prices require a quote. PCB-side components are listed on the PCB assembly tab.',[22,8,42,45,12,12,22,22,70,80,14]);
setup(assembly,aheaders,arows,'G','H','M','AirMon PCB assembly BOM','Prototype r0.1. J5 uses the socket supplied with the optional PM module. SHT40 must remain free of flux residue and coatings.',[16,8,36,24,34,24,12,12,25,20,65,80,14]);
const check=await wb.inspect({kind:'table',range:'Purchasing!A4:H8',include:'values,formulas',tableMaxRows:5,tableMaxCols:8,maxChars:1800});
console.log(check.ndjson);
const errors=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!|#SPILL!|#CALC!',options:{useRegex:true,maxResults:10},summary:'BOM formula errors'});
console.log(errors.ndjson);
await fs.mkdir(path.join(root,'tmp/bom'),{recursive:true});
for(const [s,range,name] of [[purchasing,'A4:H10','purchasing'],[assembly,'A4:J10','assembly']]){
 const preview=await wb.render({sheetName:s.name,range,scale:1,format:'png'});
 await fs.writeFile(path.join(root,`tmp/bom/${name}.png`),new Uint8Array(await preview.arrayBuffer()));
}
const file=await SpreadsheetFile.exportXlsx(wb);await file.save(path.join(root,'hardware/BOM.xlsx'));
// Machine-readable CSV exports use the same authored workbook values.
const quote=v=>{if(v instanceof Date)v=v.toISOString().slice(0,10);return '"'+String(v??'').replaceAll('"','""')+'"'};
for(const [s,last,count,name] of [[purchasing,'K',prows.length,'purchasing-bom'],[assembly,'M',arows.length,'assembly-bom']]){
 const rows=s.getRange(`A4:${last}${count+4}`).values;
 await fs.writeFile(path.join(root,`hardware/${name}.csv`),rows.map(r=>r.map(quote).join(',')).join('\r\n')+'\r\n');
}
console.log('Saved BOM workbook and both CSV exports');
