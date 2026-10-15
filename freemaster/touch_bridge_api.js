/*
 * Copyright 2024, 2026 NXP
 *
 * NXP Confidential. This software is owned or controlled by NXP and may
 * only be used strictly in accordance with the applicable license terms.
 * By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that
 * you have read, and that you agree to comply with and are bound by,
 * such license terms.  If you do not agree to be bound by the applicable
 * license terms, then you may not retain, install, activate or otherwise
 * use the software.
 *
 * Project: FreeMASTER Lite (Node.js Server)
 * mock-api.js: Mock API implementation
 */

/* eslint-disable no-undef */
/* eslint-disable no-loss-of-precision */
/* eslint-disable no-unused-vars */

'use strict';

let activeConnection = undefined; // active connection name or connection string
let namedConnections = {};        // connections defined in project file
let projectVariables = {};        // variables defined in config file
let scriptVariables = {};         // variables defined at runtime
const oscilloscopes = {};         // oscilloscope instances
const recorders = {};             // recorder instances
const memory = {};                // mock memory data (variables values) stored as JS ArrayBuffer
const boardInformation = {
  'protVer': 4,
  'cfgFlags': 0,
  'dataBusWdt': 1,
  'globVerMajor': 2,
  'globVerMinor': 0,
  'cmdBuffSize': 60,
  'recBuffSize': 4096,
  'recTimeBase': 16414,
  'descr': [83, 51, 50, 120, 120, 32, 70, 114, 101, 101, 77, 65, 83, 84, 69, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0]
};
const configParamU8 = {
  'F1': 0x2,
  'PC': Math.floor(getRandomNumber(0, 256))
};
const configParamULEB = {
  'MTU': 240,
  'BA': 0x20000000
};
const configParamString = {
  'VS': '3.0.4',
  'NM': 'Demo App',
  'DS': 'Demo application with mocked API',
  'BD': new Date().toLocaleString()
};

const MAX_BUFFER_SIZE = 256;
const VALID_SIZES = [1, 2, 4, 8];
const TYPE_MAPPING = {
  'int8': { min: -128, max: 127, type: Int8Array },
  'int16': { min: -32768, max: 32767, type: Int16Array },
  'int32': { min: -2147483648, max: 2147483647, type: Int32Array },
  'int64': { min: -2147483648, max: 2147483647, type: BigInt64Array },
  'uint8': { min: 0, max: 255, type: Uint8Array },
  'uint16': { min: 0, max: 65535, type: Uint16Array },
  'uint32': { min: 0, max: 4294967295, type: Uint32Array },
  'uint64': { min: 0, max: 4294967295, type: BigUint64Array },
  'float': { min: -4294967295, max: 4294967295, type: Float32Array },
  'double': { min: -4294967295, max: 4294967295, type: Float64Array }
};

/**
 * Returns a successfully executed command response data
 * @param {object} param0 Data depending on executed command
 */
function ResponseData({ data = {} } = {}) {
  return { 'success': true, 'data': data };
}

/**
 * Returns a failed command error
 * @param {object} param0 Error object containing an error code and message
 */
function ResponseError({ code = 0x80000000, message = '' } = {}) {
  return { 'success': false, 'error': { 'code': code, message: message || 'Something went wrong...' } };
}

/**
 * Checks whether variable size is valid
 * @param {number} size
 * @returns Returns `true` if size is valid, `false` - overwise
 */
function isSizeValid(size) {
  return VALID_SIZES.includes(size);
}

/**
 * Gets project or user defined variable information
 * @param {string} name Variable name
 * @returns Variable information object
 */
function getVariableInfo(name) {
  if (scriptVariables.hasOwnProperty(name)) {
    if (scriptVariables[name].addr === undefined) {
      scriptVariables[name].addr = scriptVariables[name].alias;
    }
    return scriptVariables[name];
  }
  if (projectVariables.hasOwnProperty(name)) {
    if (projectVariables[name].addr === undefined) {
      projectVariables[name].addr = projectVariables[name].alias;
    }
    return projectVariables[name];
  }
  return undefined;
}

/**
 * Gets variable type as string
 * @param {object} varInfo Variable Information
 * @returns Type name (as defined in TYPE_MAPPING)
 */
function getVariableType(varInfo) {
  let type = varInfo.type;
  if (type === 'int' || type === 'uint') {
    type = type + (varInfo.size * 8);
  }
  return type;
}

/**
 * Generates a random number between 2 limits
 * @param {number} min Lower limit
 * @param {*number} max Upper limit
 * @returns Random number
 */
function getRandomNumber(min, max) {
  return Math.random() * (max - min) + min;
}

/**
 * Generates a TypedArray of specified length with random values
 * @param {number | string} addr Memory address
 * @param {string} type Data type
 * @param {number} length Typed array length
 */
function readMemory(addr, type, length) {
  let buffer = new ArrayBuffer(TYPE_MAPPING[type].type.BYTES_PER_ELEMENT * length);
  let array = new TYPE_MAPPING[type].type(buffer);
  // generate random data
  for (let i = 0; i < length; i++) {
    if (type === 'uint64' || type === 'int64') {
      array[i] = BigInt(Math.floor(getRandomNumber(TYPE_MAPPING[type].min, TYPE_MAPPING[type].max)));
    } else {
      array[i] = getRandomNumber(TYPE_MAPPING[type].min, TYPE_MAPPING[type].max);
    }
  }
  // use existing data if memory was previously written
  if (memory.hasOwnProperty(addr)) {
    let viewSrc = new Uint8Array(memory[addr]);
    let viewDest = new Uint8Array(buffer);
    for (let i = 0; i < viewSrc.length; ++i) {
      viewDest[i] = viewSrc[i];
    }
  }
  return array;
}

function API() {
}

API.prototype.GetAppVersion = function() {
  return ResponseData({ data: 'FreeMASTER Lite Demo' });
};

API.prototype.Exit = function() {
  process.exit();
};

API.prototype.EnumCommPorts = function(index) {
  let connectionName = Object.keys(namedConnections)[index];
  if (namedConnections.hasOwnProperty(connectionName)) {
    return ResponseData({ data: connectionName });
  }
  return ResponseError();
};

API.prototype.GetCommPortInfo = function(name) {
  name = name || activeConnection;
  if (namedConnections.hasOwnProperty(name)) {
    return ResponseData({ data: namedConnections[name] });
  }
  return ResponseError();
};

// Start communication and set activeConnection
API.prototype.StartComm = function(name) {
  activeConnection = name;
  return ResponseData();
};

// Stop communication and set activeConnection to undefined
API.prototype.StopComm = function() {
  activeConnection = undefined;
  return ResponseData();
};

API.prototype.IsCommPortOpen = function() {
  return ResponseData({ data: activeConnection != undefined });
};

API.prototype.IsBoardDetected = function() {
  return ResponseData({ data: activeConnection !== undefined });
};

API.prototype.GetDetectedBoardInfo = function() {
  if (activeConnection) {
    return ResponseData({ data: boardInformation });
  }
  return ResponseError({ message: 'Board not connected' });
};

API.prototype.GetConfigParamU8 = function(name) {
  configParamU8['RC'] = Object.keys(recorders).length;
  configParamU8['SC'] = Object.keys(oscilloscopes).length;
  if (configParamU8.hasOwnProperty(name)) {
    return ResponseData({ data: configParamU8[name] });
  }
  return ResponseError({ code: 0x80000089, message: 'Access to target resource is denied.' });
};

API.prototype.GetConfigParamULEB = function(name) {
  if (configParamULEB.hasOwnProperty(name)) {
    return ResponseData({ data: configParamULEB[name] });
  }
  return ResponseError({ code: 0x80000089, message: 'Access to target resource is denied.' });
};

API.prototype.GetConfigParamString = function(name, len) {
  if (len == undefined || len < 0 || len > MAX_BUFFER_SIZE) {
    len = MAX_BUFFER_SIZE;
  }
  if (configParamString.hasOwnProperty(name)) {
    return ResponseData({ data: configParamString[name].substring(0, len) });
  }
  return ResponseError({ code: 0x80000089, message: 'Access to target resource is denied.' });
};

API.prototype.Authenticate = function(_pass, _access) {
  return ResponseData();
};

API.prototype.ReadIntVariable = function(addr, size) {
  if (!isSizeValid(size)) {
    return ResponseError({ message: 'Invalid element size: "' + size + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let variable = readMemory(addr, `int${size * 8}`, 1)[0];
  return ResponseData({ data: Object.prototype.toString.call(variable) === '[object BigInt]' ? variable.toString() : variable });
};

API.prototype.ReadUIntVariable = function(addr, size) {
  if (!isSizeValid(size)) {
    return ResponseError({ message: 'Invalid element size: "' + size + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let variable = readMemory(addr, `uint${size * 8}`, 1)[0];
  return ResponseData({ data:  Object.prototype.toString.call(variable) === '[object BigInt]' ? variable.toString() : variable });
};

API.prototype.ReadFloatVariable = function(addr) {
  let variable = readMemory(addr, 'float', 1)[0];
  return ResponseData({ data: variable });
};

API.prototype.ReadDoubleVariable = function(addr) {
  let variable = readMemory(addr, 'double', 1)[0];
  return ResponseData({ data: variable });
};

API.prototype.WriteIntVariable = function(addr, size, data) {
  if (!isSizeValid(size)) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  let array = new TYPE_MAPPING[`int${size * 8}`].type([data]);
  memory[addr] = array.buffer;
  return ResponseData();
};

API.prototype.WriteUIntVariable = function(addr, size, data) {
  if (!isSizeValid(size)) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  let array = new TYPE_MAPPING[`uint${size * 8}`].type([data]);
  memory[addr] = array.buffer;
  return ResponseData();
};

API.prototype.WriteFloatVariable = function(addr, data) {
  let array = new TYPE_MAPPING['float'].type([data]);
  memory[addr] = array.buffer;
  return ResponseData();
};

API.prototype.WriteDoubleVariable = function(addr, data) {
  let array = new TYPE_MAPPING['double'].type([data]);
  memory[addr] = array.buffer;
  return ResponseData();
};

API.prototype.ReadMemory = function(addr, size) {
  if (size < 0) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  let array = Array.from(readMemory(addr, 'uint8', size));
  return ResponseData({ data: array });
};

API.prototype.ReadIntArray = function(addr, size, elSize) {
  if (size < 0) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  if (!isSizeValid(elSize)) {
    return ResponseError({ message: 'Invalid element size: "' + elSize + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let array = Array.from(readMemory(addr, `int${elSize * 8}`, size));
  return ResponseData({ data: Array.isArray(array) && Object.prototype.toString.call(array[0]) === '[object BigInt]' ? array.map(el => el.toString()) : array });
};

API.prototype.ReadUIntArray = function(addr, size, elSize) {
  if (size < 0) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  if (!isSizeValid(elSize)) {
    return ResponseError({ message: 'Invalid element size: "' + elSize + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let array = Array.from(readMemory(addr, `uint${elSize * 8}`, size));
  return ResponseData({ data: Array.isArray(array) && Object.prototype.toString.call(array[0]) === '[object BigInt]' ? array.map(el => el.toString()) : array });
};

API.prototype.ReadFloatArray = function(addr, size) {
  if (size < 0) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  let array = Array.from(readMemory(addr, 'float', size));
  return ResponseData({ data: array });
};

API.prototype.ReadDoubleArray = function(addr, size) {
  if (size < 0) {
    return ResponseError({ message: 'Invalid size: ' + size });
  }
  let array = Array.from(readMemory(addr, 'double', size));
  return ResponseData({ data: array });
};

API.prototype.WriteMemory = function(addr, data) {
  let array = new TYPE_MAPPING['uint8'].type(data);
  memory[addr] = array.buffer;
  return ResponseData({ data: array.length });
};

API.prototype.WriteIntArray = function(addr, elSize, data) {
  if (!isSizeValid(elSize)) {
    return ResponseError({ message: 'Invalid element size: "' + elSize + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let array = new TYPE_MAPPING[`int${elSize * 8}`].type(data);
  memory[addr] = array.buffer;
  return ResponseData({ data: array.length });
};

API.prototype.WriteUIntArray = function(addr, elSize, data) {
  if (!isSizeValid(elSize)) {
    return ResponseError({ message: 'Invalid element size: "' + elSize + '". Allowed sizes are 1, 2, 4, and 8 bytes.' });
  }
  let array = new TYPE_MAPPING[`uint${elSize * 8}`].type(data);
  memory[addr] = array.buffer;
  return ResponseData({ data: array.length });
};

API.prototype.WriteFloatArray = function(addr, data) {
  let array = new TYPE_MAPPING['float'].type(data);
  memory[addr] = array.buffer;
  return ResponseData({ data: array.length });
};

API.prototype.WriteDoubleArray = function(addr, data) {
  let array = new TYPE_MAPPING['double'].type(data);
  memory[addr] = array.buffer;
  return ResponseData({ data: array.length });
};

API.prototype.ReadELF = function(_elfFile) {
  return ResponseData({ data: { count: 0 } });
};

API.prototype.ReadTSA = function() {
  return ResponseData({ data: { count: 0 } });
};

API.prototype.EnumSymbols = function(_index) {
  return ResponseError();
};

API.prototype.GetSymbolInfo = function(_name) {
  return ResponseError();
};

API.prototype.EnumVariables = function(index) {
  let allVariables = {
    ...projectVariables,
    ...scriptVariables
  };
  let name = Object.keys(allVariables)[index];
  if (allVariables.hasOwnProperty(name)) {
    return ResponseData({ data: name });
  }
  return ResponseError();
};

API.prototype.GetVariableInfo = function(name) {
  return ResponseData({ data: getVariableInfo(name) });
};

API.prototype.DefineVariable = function(varInfo) {
  try {
    processVarInfo(varInfo);
    if (varInfo.alias)
      varInfo.addr = varInfo.alias;
    scriptVariables[varInfo.name] = varInfo;
    return ResponseData();
  } catch (err) {
    return ResponseError({ message: err });
  }
};

API.prototype.DeleteVariable = function(name) {
  if (scriptVariables.hasOwnProperty(name)) {
    delete scriptVariables[name];
  }
  return ResponseData();
};

API.prototype.DeleteAllScriptVariables = function() {
  scriptVariables = {};
  return ResponseData();
};

API.prototype.ReadVariable = function(name) {
  let varInfo = getVariableInfo(name);
  if (varInfo === undefined) {
    return ResponseError({ message: 'Unknown variable: "' + name + '".' });
  }
  if (varInfo.addr === undefined) {
    return ResponseError({ message: 'Unknown address of variable: "' + name + '".' });
  }
  const values = readMemory(varInfo.addr, getVariableType(varInfo), varInfo.array_length === undefined ? 1 : varInfo.array_length);
  const processed = Array.from(values.map(val => Object.prototype.toString.call(val) === '[object BigInt]' ? val.toString() : val));
  return ResponseData({ data: varInfo.array_length === undefined ? processed[0] : processed });
};

API.prototype.WriteVariable = function(name, value) {
  let varInfo = getVariableInfo(name);
  if (varInfo === undefined) {
    return ResponseError({ message: 'Unknown variable: "' + name + '".' });
  }
  if (varInfo.addr === undefined) {
    return ResponseError({ message: 'Unknown address of variable: "' + name + '".' });
  }
  if (varInfo.array_length === undefined) {
    if (Array.isArray(value)) {
      return ResponseError({ message: 'Cannot write an array data into a variable of type ' + varInfo.type + '.' });
    } else {
      let array = new TYPE_MAPPING[getVariableType(varInfo)].type([value]);
      memory[varInfo.addr] = array.buffer;
      return ResponseData();
    }
  }
  if (Array.isArray(value) && value.length <= varInfo.array_length) {
    let array = new TYPE_MAPPING[getVariableType(varInfo)].type(value);
    memory[varInfo.addr] = array.buffer;
    return ResponseData();
  } else {
    return ResponseError({ message: 'Provided value is not an array or its size exceeds defined array length.' });
  }
};

API.prototype.ReadMultipleVariables = function(names) {
  let data = [];
  for (let i = 0; i < names.length; ++i) {
    let response = this.ReadVariable(names[i]);
    data[i] = { name: names[i], status: response.success };
    if (response.success) {
      data[i].value = response.data;
    }
  }
  return ResponseData({ data: data });
};

API.prototype.WriteMultipleVariables = function(values) {
  let data = [];
  for (let i = 0; i < values.length; ++i) {
    let response = this.WriteVariable(values[i].name, values[i].value);
    data[i] = { name: values[i].name, status: response.success };
  }
  return ResponseData({ data: data });
};

API.prototype.SetupOscilloscope = function(id, vars) {
  let oscilloscope = { variables: [] };
  //Create oscilloscope configuration object
  for (let i = 0; i < vars.length; ++i) {
    let varInfo = getVariableInfo(vars[i]);
    if (varInfo === undefined) {
      return ResponseError({ message: 'Unknown variable: "' + vars[i] + '".' });
    }
    oscilloscope.variables.push(varInfo.name);
  }
  oscilloscopes[id] = oscilloscope;
  return ResponseData();
};

API.prototype.GetOscilloscopeData = function(id) {
  if (!oscilloscopes.hasOwnProperty(id)) {
    return ResponseError({ message: 'Oscilloscope "' + id + '" not found.' });
  }
  let oscData = [];
  for (let i = 0; i < oscilloscopes[id].variables.length; ++i) {
    let name = oscilloscopes[id].variables[i];
    let varInfo = getVariableInfo(name);
    let variable = readMemory(varInfo.addr, getVariableType(varInfo), 1)[0];
    oscData.push( Object.prototype.toString.call(variable) === '[object BigInt]' ? variable.toString() : variable);
  }
  return ResponseData({ data: oscData });
};

API.prototype.GetRecorderLimits = function(id) {
  if (!recorders.hasOwnProperty(id)) {
    return ResponseError({ code: 0x80000094, message: ' No resource instance at given index. ' });
  }
  return ResponseData({ data: { baseRateSec: 0, buffSize: 1024, recStructSize: 64, varStructSize: 40 } });
};

API.prototype.SetupRecorder = function(id, config, vars) {
  // Create middleware helper object (stores pointers to variables, point size and total number of points)
  let recorder = { variables: [], pointsPreTrigger: config.pointsPreTrigger, pointsTotal: config.pointsTotal, timeDiv: config.timeDiv, Status: 1 };
  // Create recorder variables array
  for (let i = 0; i < vars.length; ++i) {
    let recVar = {};
    let varInfo = getVariableInfo(vars[i].name);
    if (varInfo === undefined) {
      return ResponseError({ message: 'Unknown variable: "' + vars[i].name + '".' });
    }
    recVar.name = varInfo.name;

    // Init variable to be kept on the middleware
    if (vars[i].trgType && vars[i].trgThr) {
      recVar.trgType = vars[i].trgType;
      recVar.trgThr = vars[i].trgThr;
    }
    recorder.variables.push(recVar);
  }
  recorders[id] = recorder;
  return ResponseData();
};

API.prototype.StartRecorder = function(id) {
  if (!recorders.hasOwnProperty(id)) {
    return ResponseError({ message: 'Recorder ' + id + ' not found.' });
  }
  recorders[id].Status = 2;
  return ResponseData();
};

API.prototype.StopRecorder = function(id) {
  if (!recorders.hasOwnProperty(id)) {
    return ResponseError({ message: 'Recorder ' + id + ' not found.' });
  }
  recorders[id].Status = 3;
  return ResponseData();
};

API.prototype.GetRecorderStatus = function(id) {
  if (!recorders.hasOwnProperty(id)) {
    return ResponseError({ message: 'Recorder ' + id + ' not found.' });
  }
  return ResponseData({ data: recorders[id].Status });
};

API.prototype.GetRecorderData = function(id) {
  if (!recorders.hasOwnProperty(id)) {
    return ResponseError({ message: 'Recorder ' + id + ' not found.' });
  }
  let length = recorders[id].pointsTotal;
  let recData = new Array();
  recorders[id].variables.forEach(variable => {
    let varInfo = getVariableInfo(variable.name);
    if (!(variable.trgType && variable.trgType === 0x04)) {
      let array = Array.from(readMemory(varInfo.addr, getVariableType(varInfo), length));
      recData.push((Array.isArray(array) && Object.prototype.toString.call(array[0]) === '[object BigInt]') ? array.map(el => el.toString()) : array);
    }
  });
  return ResponseData({ data: recData });
};

API.prototype.PipeSetDefaultRxMode = function(_rxAllOrNothing, _rxTimeout_ms) {
  return ResponseData();
};
API.prototype.PipeSetDefaultTxMode = function(_txAllOrNothing) {
  return ResponseData();
};

API.prototype.PipeSetDefaultStringMode = function(_unicode) {
  return ResponseData();
};

API.prototype.PipeOpen = function(_port, _txBufferSize, _rxBufferSize) {
  return ResponseData();
};

API.prototype.PipeClose = function(_port) {
  return ResponseData();
};

API.prototype.PipeGetRxBufferSize = function(_port) {
  return ResponseData();
};

API.prototype.PipeGetTxBufferSize = function(_port) {
  return ResponseData();
};

API.prototype.PipeFlush = function(_port, _timeout_ms) {
  return ResponseData();
};

API.prototype.PipeGetRxBytes = function(_port) {
  return ResponseData();
};

API.prototype.PipeGetTxBytes = function(_port) {
  return ResponseData();
};

API.prototype.PipeGetTxFree = function(_port) {
  return ResponseData();
};

API.prototype.PipeWriteString = function(_port, _str, _allOrNothing, _unicode) {
  return ResponseData();
};

API.prototype.PipeWriteIntArray = function(_port, _elSize, _data, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeWriteUIntArray = function(_port, _elSize, _data, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeWriteFloatArray = function(_port, _data, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeWriteDoubleArray = function(_port, _data, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeReadString = function(_port, _rxTimeout_ms, _charsToRead, _allOrNothing, _unicode) {
  return ResponseData();
};

API.prototype.PipeReadIntArray = function(_port, _elSize, _rxTimeout_ms, _size, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeReadUIntArray = function(_port, _elSize, _rxTimeout_ms, _size, _allOrNothing) {
  return ResponseData();
};
API.prototype.PipeReadFloatArray = function(_port, _rxTimeout_ms, _size, _allOrNothing) {
  return ResponseData();
};

API.prototype.PipeReadDoubleArray = function(_port, _rxTimeout_ms, _size, _allOrNothing) {
  return ResponseData();
};

API.prototype.LocalFileOpen = function(_file, _mode) {
  return ResponseData();
};

API.prototype.LocalFileClose = function(_handle) {
  return ResponseData();
};

API.prototype.LocalFileReadString = function(_handle, _charsToRead, _unicode) {
  return ResponseData();
};

API.prototype.LocalFileWriteString = function(_handle, _str, _unicode) {
  return ResponseData();
};

API.prototype.LogEnable = function(_name, _file) {
  return ResponseData();
};

API.prototype.LogDisable = function() {
  return ResponseData();
};

API.prototype.LogSetPattern = function(_pattern) {
  return ResponseData();
};

API.prototype.LogSetVerbosity = function(_verbosity) {
  return ResponseData();
};

API.prototype.LogSetServices = function(_mask, _services) {
  return ResponseData();
};

function exportAPI(connections, _fileAccessRules, variables) {
  namedConnections = connections;
  projectVariables = variables;
  return new API();
}

exports = module.exports = exportAPI;