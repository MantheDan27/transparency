const fs = require('fs');
const code = fs.readFileSync('device-monitor-desktop/src/renderer.js', 'utf8');

// find all labels
const labels = code.match(/<label[^>]*>.*?<\/label>/gs) || [];
const labelsWithoutFor = labels.filter(l => !l.includes('for='));
console.log("Renderer labels without for attribute:");
console.log(labelsWithoutFor);
