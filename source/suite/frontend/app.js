(function (global) {
  'use strict';
  const PROTOCOL = 1;
  const MODES = { press: 0x1000, release: 0x2000, hold: 0x3000 };
  const TYPES = new Set(['toggle','choice','text','number','range','button','label','heading','hotkey']);
  const aliases = { checkbox: 'toggle', select: 'choice', combobox: 'choice', list: 'choice', slider: 'range', tab: 'choice' };
  const pageKey = (module, category = 0) => module === 2 ? `2/${category}` : String(module);
  const controlKey = (module, id) => `${module}:${id}`;
  const safeInteger = value => Number.isSafeInteger(value) && value >= 0;
  function placeAfter(container,node,after) { if(node.parentNode!==container || node.previousSibling!==after) container.insertBefore(node,after?after.nextSibling:container.firstChild); }
  function validText(value,maxBytes=65536) {
    if (typeof value !== 'string') return false;
    let bytes=0;
    for(let index=0;index<value.length;index++) {
      const point=value.charCodeAt(index);
      if(point===0) return false;
      if(point>=0xd800 && point<=0xdbff) { const low=value.charCodeAt(++index); if(!(low>=0xdc00 && low<=0xdfff)) return false; bytes+=4; }
      else if(point>=0xdc00 && point<=0xdfff) return false;
      else bytes+=point<0x80?1:point<0x800?2:3;
      if(bytes>maxBytes) return false;
    }
    return true;
  }
  const stateCategory = message => message.module === 2 ? Number(String(message.page || '').match(/(?:^|\/)([1-5])$/)?.[1] || message.category || 0) : 0;
  const DEBUG_STATES = new Set(['ready','blocked','unknown','unused']);
  const DEBUG_HOOKS = new Set(['na','inactive','installed','pending','conflict','unknown']);
  const DEBUG_STALE_MS = 5000;
  const uint32 = value => Number.isInteger(value) && value >= 0 && value <= 4294967295;
  const debugText = value => typeof value === 'string' ? value : '';
  function canonicalDebug(value) {
    if (!value || value.present !== true) return null;
    const valid = uint32(value.siteId) && uint32(value.address) && uint32(value.ageMs);
    return Object.freeze({present:true,siteId:uint32(value.siteId)?value.siteId:0,address:uint32(value.address)?value.address:0,
      kind:debugText(value.kind),purpose:debugText(value.purpose),features:debugText(value.features),
      state:DEBUG_STATES.has(value.state)?value.state:'unknown',detail:debugText(value.detail),
      hook:DEBUG_HOOKS.has(value.hook)?value.hook:'unknown',ageMs:uint32(value.ageMs)?value.ageMs:0,sampled:valid && value.sampled===true});
  }
  const debugAddress = value => `0x${(uint32(value)?value:0).toString(16).toUpperCase().padStart(8,'0')}`;
  function debugStatus(control,receivedAt=Date.now(),now=Date.now(),current=true) {
    const d=control.debug || {}, elapsed=Math.max(0,now-receivedAt), ageMs=(d.ageMs || 0)+elapsed;
    const sampled=d.sampled===true, stale=sampled && ageMs>DEBUG_STALE_MS;
    const state=d.state==='unused'?'unused':!current || !sampled || stale?'unknown':DEBUG_STATES.has(d.state)?d.state:'unknown';
    const call=['call','native','native-call'].includes(d.kind) || d.hook==='na';
    const label=state==='ready'?(call?'可调用':'已就绪'):state==='blocked'?'受阻':state==='unused'?'未使用':!current?'等待当前页状态':stale?'观测已过期':sampled?'状态待确认':'尚未观测';
    const hook=d.hook==='na'?'na':!current || !sampled || stale?'unknown':DEBUG_HOOKS.has(d.hook)?d.hook:'unknown';
    const hookLabel={na:'无需 Hook',inactive:'Hook 未启用',installed:'Hook 已安装',pending:'Hook 等待处理',conflict:'Hook 冲突',unknown:'Hook 尚未观测'}[hook];
    return {state,label,hook,hookLabel,ageMs,sampled,stale,tone:state==='ready'?'ready':state==='blocked'?'blocked':'neutral'};
  }
  const debugKindLabel = kind => ({call:'原生调用',native:'原生调用','native-call':'原生调用',patch:'指令修改',breakpoint:'硬件断点',system:'Windows 消息入口'})[kind] || kind;
  const debugToggleLabel = (control,pending=false) => control.debug?.kind==='system'?'无需签名校验':control.debug?.state==='unused'?'本版本未启用':pending?'等待应用':control.checked?'校验开启':'校验关闭';
  function filterDebugControls(controls,{search='',group='',state='',receivedAt=Date.now(),now=Date.now(),current=true}={}) {
    const query=search.trim().toLocaleLowerCase();
    return controls.filter(control=>{
      if (!control.visible || !control.debug?.present) return false;
      const d=control.debug, status=debugStatus(control,receivedAt,now,current);
      if (group && (control.group || '未分组')!==group) return false;
      if (state==='unchecked' ? control.checked : state==='attention' ? !(status.state==='blocked' || status.hook==='conflict' || status.stale) : state && status.state!==state) return false;
      return !query || [control.label,control.group,debugAddress(d.address),String(d.address),d.kind,d.purpose,d.features,d.detail,status.label,status.hookLabel].join(' ').toLocaleLowerCase().includes(query);
    });
  }
  function canonical(control) {
    if (!control || !safeInteger(control.id) || !control.id || control.id>4294967295) return null;
    const type = aliases[control.type] || control.type;
    const result = { ...control, type: TYPES.has(type) ? type : 'label', label: String(control.label || ''), text: String(control.text || ''), group: String(control.group || ''), hint: String(control.hint || ''),
      options: Array.isArray(control.options) ? control.options.map(String).slice(0, 65536) : [],
      optionEnabled: Array.isArray(control.optionEnabled) ? control.optionEnabled.map(value => value !== false).slice(0, 65536) : [],
      selected: Number.isInteger(control.selected) ? control.selected : -1, checked: Boolean(control.checked), enabled: control.enabled !== false,
      visible: control.visible !== false, readonly: Boolean(control.readonly), runtimeError: Boolean(control.runtimeError), groupHeader: Boolean(control.groupHeader), collapsible: Boolean(control.collapsible), collapsedDefault: Boolean(control.collapsedDefault), immediate: Boolean(control.immediate), min: Number.isFinite(control.min) ? control.min : 0,
      actionRole: ['primary','secondary','danger'].includes(control.actionRole) ? control.actionRole : 'secondary',
      max: Number.isFinite(control.max) ? control.max : 100, step: Number.isFinite(control.step) && control.step > 0 ? control.step : 1,
      row: Number.isFinite(control.row) ? control.row : 0, column: Number.isFinite(control.column) ? control.column : 0, presentation: String(control.presentation || ''), debug: canonicalDebug(control.debug) };
    if (!TYPES.has(type)) { result.readonly = true; result.hint ||= '此设置目前仅供查看。'; }
    return Object.freeze(result);
  }
  function actualValue(control, action) {
    if (action === 'toggle') return control.checked;
    if (action === 'select' || action === 'hotkey') return control.selected;
    if (action === 'range') { const text = control.text.trim(), value = Number(text); return text && Number.isFinite(value) ? value : control.selected >= 0 ? control.selected : control.min; }
    return control.text;
  }
  class ConsoleStore {
    constructor(send = () => {}, changed = () => {}, options = {}) {
      this.send = send; this.changed = changed; this.pages = new Map(); this.pending = new Map(); this.drafts = new Map(); this.errors = new Map(); this.generation = new Map();
      this.current = { module: 0, category: 0 }; this.shell = { version: '1.0.41-pre.9', modules: [], closing: false }; this.seq = options.initialSeq ?? Date.now() * 1024; this.connected = false; this.closed = false; this.notice=''; this.configRetryPending=0;
      if (!safeInteger(this.seq) || this.seq > Number.MAX_SAFE_INTEGER - 1024) throw new RangeError('Invalid initial action sequence');
    }
    emit(message) { this.send({ ...message, protocol: PROTOCOL }); }
    ready() { this.emit({ kind: 'ready' }); }
    navigate(module, category = 0) {
      if (!Number.isInteger(module) || module < 0 || module > 2 || (module === 2 && (!Number.isInteger(category) || category < 1 || category > 5))) return false;
      this.current = { module, category: module === 2 ? category : 0 }; this.emit({ kind: 'navigate', ...this.current }); this.changed('navigate'); return true;
    }
    get state() { return this.pages.get(pageKey(this.current.module, this.current.category)) || null; }
    getControl(id) { return this.state?.controls.find(control => control.id === id) || null; }
    receive(message) {
      if (!message || typeof message !== 'object' || message.protocol !== PROTOCOL) return false;
      if(message.kind==='close-check') {
        if(!safeInteger(message.seq) || !message.seq) return false;
        this.emit({kind:'close',seq:message.seq,unsubmitted:Math.min(10000,this.drafts.size+this.pending.size)});
        return true;
      }
      if (message.kind === 'shell') {
        this.connected = true; this.shell = { ...this.shell, ...message, modules: Array.isArray(message.modules) ? message.modules : this.shell.modules }; this.closed = Boolean(message.closing);
        if(!this.shell.config?.failed || (safeInteger(this.shell.config?.retrySeq) && this.shell.config.retrySeq>=this.configRetryPending)) this.configRetryPending=0;
        this.changed('shell'); return true;
      }
      if (message.kind === 'state') {
        if (!safeInteger(message.module) || message.module > 2 || !safeInteger(message.generation) || !message.generation || !safeInteger(message.revision) || !Array.isArray(message.controls) || message.controls.length > 10000) return false;
        if (message.generation < (this.generation.get(message.module) || 0)) return false;
        const category = stateCategory(message); if (message.module === 2 && (category < 1 || category > 5)) return false;
        const key = pageKey(message.module, category), previous = this.pages.get(key);
        if (previous && previous.generation === message.generation && previous.revision >= message.revision) return false;
        const controls = message.controls.map(canonical);
        if (controls.some(control => !control) || new Set(controls.map(control => control.id)).size !== controls.length) return false;
        const ids=new Map(controls.map(control=>[control.id,control]));
        if(previous) for(const old of previous.controls) if(!ids.has(old.id)) {
          const removedKey=controlKey(message.module,old.id);
          let changed=false;
          if(this.pending.get(removedKey)?.page===key) {this.pending.delete(removedKey);changed=true;}
          if(this.drafts.get(removedKey)?.page===key) {this.drafts.delete(removedKey);changed=true;}
          if(changed) this.notice='部分设置已更新，相关操作未能确认；请检查当前值后重试。';
          this.errors.delete(removedKey);
        }
        if (message.generation > (this.generation.get(message.module) || 0)) {
          let unconfirmed=false;
          this.generation.set(message.module, message.generation);
          for (const [key, pending] of this.pending) if (pending.module === message.module) {
            if(pending.action==='text' && !this.drafts.has(key) && (pending.page!==pageKey(message.module,category) || ids.get(pending.id)?.text!==pending.value)) this.drafts.set(key,{value:pending.value,editing:false,page:pending.page});
            this.pending.delete(key);unconfirmed=true;
          }
          for (const [key,draft] of this.drafts) if (key.startsWith(`${message.module}:`) && draft.page===pageKey(message.module,category)) { const control=ids.get(Number(key.split(':')[1])); if(!control || !['text','number','choice','range'].includes(control.type) || control.readonly) { this.drafts.delete(key); unconfirmed=true; } }
          for (const [key] of this.errors) if (key.startsWith(`${message.module}:`)) this.errors.delete(key);
          if(unconfirmed) this.notice='设置列表已更新，部分操作未能确认；请检查当前值后重试。';
        }
        const state = Object.freeze({ ...message, page: key, controls: Object.freeze(controls), title: String(message.title || ''), status: String(message.status || ''), receivedAt: Date.now() });
        this.pages.set(key, state); this.connected = true;
        for (const control of controls) {
          const key = controlKey(message.module, control.id), pending = this.pending.get(key);
          if (pending && pending.generation === message.generation && message.revision > pending.revision) {
            pending.observedRevision=message.revision;
            if(pending.acknowledged && pending.requiredRevision>0 ? message.revision>=pending.requiredRevision : !['press','activate'].includes(pending.action) && (pending.acknowledged || actualValue(control,pending.action)===pending.value)) this.pending.delete(key);
          }
        }
        this.changed('state'); return true;
      }
      if (message.kind === 'ack') {
        if (typeof message.ok !== 'boolean') return false;
        const key = controlKey(message.module, message.id), pending = this.pending.get(key);
        if (!pending || message.seq !== pending.seq || message.generation !== pending.generation) return false;
        if (message.ok) {
          pending.acknowledged=true;pending.requiredRevision=safeInteger(message.revision)?message.revision:0;
          const currentPage=this.pages.get(pending.page),current=currentPage?.controls.find(control=>control.id===message.id);
          if(pending.requiredRevision>0 ? currentPage?.generation===pending.generation && currentPage.revision>=pending.requiredRevision : pending.action==='press' || pending.action==='activate' || (current && actualValue(current,pending.action)===pending.value) || pending.observedRevision>pending.revision) this.pending.delete(key);
        }
        else { this.pending.delete(key); this.errors.set(key, String(message.error || '操作未完成，请重试。')); }
        this.changed(message.ok ? 'ack' : 'error'); return true;
      }
      return false;
    }
    beginEdit(id) { const control = this.getControl(id); if (!control) return; const key = controlKey(this.current.module, id); if (!this.drafts.has(key)) this.drafts.set(key, { value: control.text, editing: true,page:this.state.page }); else this.drafts.get(key).editing = true; this.changed('draft'); }
    draft(id, value) { const key = controlKey(this.current.module, id); this.drafts.set(key, { value, editing: true,page:this.state?.page }); }
    cancelEdit(id) { const key=controlKey(this.current.module,id);this.drafts.delete(key);this.errors.delete(key);this.changed('draft'); }
    commit(id, action = 'text') {
      const key = controlKey(this.current.module, id), draft = this.drafts.get(key); if (!draft) return false;
      const control = this.getControl(id); if (!control) return false;
      if(actualValue(control,action)===draft.value) { this.drafts.delete(key); this.changed('draft'); return false; }
      if(this.act(id,action,draft.value)) { this.drafts.delete(key); return true; }
      draft.editing=false; return false;
    }
    act(id, action, value) {
      const state = this.state, control = this.getControl(id); if (!state || !control || !control.enabled || !control.visible || this.closed || state.generation!==this.generation.get(state.module)) return false;
      const expected = { toggle: 'toggle', select: 'choice', activate: 'choice', range: 'range', press: 'button', hotkey: 'hotkey' };
      if (expected[action] && control.type !== expected[action]) return false;
      if (action === 'text' && (!['text','number','choice'].includes(control.type) || control.readonly)) return false;
      if (action === 'activate' && control.presentation !== 'list') return false;
      if (['select','activate'].includes(action) && (!Number.isInteger(value) || value < 0 || value >= control.options.length || control.optionEnabled[value] === false)) return false;
      if (action === 'toggle' && typeof value !== 'boolean') return false;
      if (['range','hotkey'].includes(action) && (!Number.isSafeInteger(value) || (action === 'hotkey' && (value < 0 || value > 65535)))) return false;
      if (action==='range' && (value < -2147483648 || value > 2147483647)) return false;
      if (action === 'text' && !validText(value)) { this.errors.set(controlKey(state.module,id),'文本过长或包含不支持的字符，编辑内容尚未提交。'); this.changed('error'); return false; }
      if (!['toggle','select','activate','range','press','hotkey','text'].includes(action)) return false;
      if (control.immediate && this.isPending(id)) return false;
      if (this.seq >= Number.MAX_SAFE_INTEGER) return false;
      const seq = this.seq+1, key = controlKey(state.module, id);
      const message = { kind: 'action', module: state.module, generation: state.generation, id, seq, action };
      if (action !== 'press') message.value = value;
      if (control.immediate) message.inputTime = Date.now();
      if(!validText(JSON.stringify({...message,protocol:PROTOCOL}),262144)) {this.errors.set(key,'编辑内容超过可发送长度，尚未提交。');this.changed('error');return false;}
      this.seq=seq;this.pending.set(key, { module: state.module, generation: state.generation, page:state.page,id,seq, action, value, revision: state.revision, sentAt: Date.now(), acknowledged: false, observedRevision:0,requiredRevision:0 }); this.errors.delete(key); this.notice='';
      this.emit(message); this.changed('action'); return true;
    }
    value(control, action) { const key = controlKey(this.current.module, control.id), draft = this.drafts.get(key), pending = this.pending.get(key); if (draft && (action === 'text' || action === 'range')) return draft.value; if (pending && pending.action === action) return pending.value; return actualValue(control, action); }
    error(id) { return this.errors.get(controlKey(this.current.module, id)) || ''; }
    isPending(id) { return this.pending.has(controlKey(this.current.module, id)); }
    retryConfig() {
      if(!this.connected || !this.shell.config?.failed || !this.shell.config.canRetry || this.configRetryPending || this.seq>=Number.MAX_SAFE_INTEGER) return false;
      this.configRetryPending=++this.seq; this.emit({kind:'config-retry',seq:this.configRetryPending}); this.changed('config-retry'); return true;
    }
    enable(module, enabled) { if (!Number.isInteger(module) || module < 0 || module > 1 || typeof enabled !== 'boolean') return false; this.emit({ kind: 'module', module, enabled }); return true; }
  }
  const SPECIAL_KEYS = { Backspace: 8, Tab: 9, Enter: 13, Shift: 16, Control: 17, Alt: 18, Pause: 19, CapsLock: 20, Escape: 27, Space: 32, PageUp: 33, PageDown: 34, End: 35, Home: 36, ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Insert: 45, Delete: 46, NumpadMultiply: 106, NumpadAdd: 107, NumpadSubtract: 109, NumpadDecimal: 110, NumpadDivide: 111, NumLock: 144, ScrollLock: 145, Semicolon: 186, Equal: 187, Comma: 188, Minus: 189, Period: 190, Slash: 191, Backquote: 192, BracketLeft: 219, Backslash: 220, BracketRight: 221, Quote: 222 };
  function virtualKey(event) { if (Number.isInteger(event.keyCode) && event.keyCode > 0 && event.keyCode <= 255 && event.keyCode !== 229) return event.keyCode; const code = event.code || ''; if (/^Key[A-Z]$/.test(code)) return code.charCodeAt(3); if (/^Digit[0-9]$/.test(code)) return code.charCodeAt(5); if (/^F(?:[1-9]|1[0-9]|2[0-4])$/.test(code)) return 111 + Number(code.slice(1)); if (/^Numpad[0-9]$/.test(code)) return 96 + Number(code.slice(6)); return SPECIAL_KEYS[code] || SPECIAL_KEYS[event.key] || (event.key?.length === 1 ? event.key.toUpperCase().charCodeAt(0) : 0); }
  function encodeHotkey(event, previous = 0) { if (event.metaKey) return null; const vk = virtualKey(event); if (!vk || vk > 255) return null; let mods = (event.shiftKey ? 0x100 : 0) | (event.ctrlKey ? 0x200 : 0) | (event.altKey ? 0x400 : 0); if (vk === 16) mods &= ~0x100; if (vk === 17) mods &= ~0x200; if (vk === 18) mods &= ~0x400; return (previous & 0xb000) | mods | vk; }
  function keyName(value) { const vk = value & 255; if (!vk) return '未绑定'; const names = { 1:'鼠标左键',2:'鼠标右键',4:'鼠标中键',5:'鼠标侧键 1',6:'鼠标侧键 2',8:'Backspace',9:'Tab',13:'Enter',16:'Shift',17:'Ctrl',18:'Alt',19:'Pause',20:'Caps Lock',27:'Esc',32:'Space',33:'Page Up',34:'Page Down',35:'End',36:'Home',37:'←',38:'↑',39:'→',40:'↓',45:'Insert',46:'Delete',144:'Num Lock',145:'Scroll Lock',186:';',187:'=',188:',',189:'-',190:'.',191:'/',192:'`',219:'[',220:'\\',221:']',222:"'" }; const prefix = []; if ((value & 0x200) && vk !== 17) prefix.push('Ctrl'); if ((value & 0x100) && vk !== 16) prefix.push('Shift'); if ((value & 0x400) && vk !== 18) prefix.push('Alt'); const name = names[vk] || (vk >= 112 && vk <= 135 ? `F${vk - 111}` : vk >= 96 && vk <= 105 ? `Num ${vk - 96}` : String.fromCharCode(vk)); return [...prefix, name].join(' + '); }
  const debugToolControls = controls => controls.filter(control => control.visible && !control.debug?.present);
  function groupControls(controls, query = '') {
    const groups=new Map(),needle=query.trim().toLocaleLowerCase();let heading='常用设置';
    for(const control of controls) {
      if(control.visible===false)continue;
      if(control.type==='heading'){heading=control.label || control.text || heading;continue;}
      if(control.type==='label' && !control.label && !control.text)continue;
      const name=control.group || heading;
      if(!groups.has(name))groups.set(name,[]);groups.get(name).push(control);
    }
    if(needle)for(const [name,items] of groups) {
      const matches=settingLines(items).filter(line=>line.some(control=>[control.label,control.text,control.hint,name,...control.options].join(' ').toLocaleLowerCase().includes(needle))).flat();
      if(matches.length)groups.set(name,matches);else groups.delete(name);
    }
    return groups;
  }
  // The adapter already exports the original logical row. Keep its related
  // editors together instead of turning every native widget into a full row.
  // Missing/default coordinates must never combine unrelated settings.
  function settingLines(controls) {
    const lines=[];
    for (const control of controls) {
      const previous=lines.at(-1), positioned=control.row>0 && control.column>0;
      if (positioned && previous && previous[0].row===control.row && previous[0].column>0) previous.push(control);
      else lines.push([control]);
    }
    return lines;
  }
  function inlineLabel(control, hasPreset=false) {
    if(control.type==='hotkey')return hasPreset?'自定义键':control.options.includes('hold')?'按住键':'快捷键';
    if(control.type==='choice') {
      if(control.options.includes('仅鼠标') && control.options.includes('自定义'))return '按键';
      if(control.options.includes('中键') && control.options.includes('侧键 1'))return '鼠标';
      return '选项';
    }
    return {number:'数值',range:'数值',text:'内容'}[control.type] || '';
  }
  function groupPresentation(controls,userCollapsed,searching=false,ackErrors=new Set(),searchCollapsed=false) {
    const hasError=controls.some(control=>control.runtimeError || ackErrors.has(control.id));
    return {hasError,collapsed:searching?Boolean(searchCollapsed):Boolean(userCollapsed),
      headers:controls.filter(control=>control.groupHeader && !['label','heading'].includes(control.type)),
      details:controls.filter(control=>!control.groupHeader)};
  }
  global.RA2ConsoleCore = Object.freeze({ ConsoleStore, pageKey, canonical, groupControls, settingLines, groupPresentation, keyName, encodeHotkey, virtualKey, validText, placeAfter, canonicalDebug, debugAddress, debugStatus, debugKindLabel, debugToggleLabel, filterDebugControls, debugToolControls, DEBUG_STALE_MS, MODES });
  if (typeof document === 'undefined') return;

  const $ = id => document.getElementById(id);
  const CATEGORIES = [ ['视频','DISPLAY & VIDEO','调整画面、光标与监控视图。','<rect x="3" y="4" width="18" height="13" rx="2"></rect><path d="M8 21h8M12 17v4"></path>'], ['控制','INPUT & CONTROL','设置操作方式、快捷键与单位命令。','<circle cx="12" cy="12" r="7"></circle><path d="M12 2v4m0 12v4M2 12h4m12 0h4"></path>'], ['自动功能','AUTOMATION','按需开启战术辅助与自动处理。','<path d="m13 2-8 12h6l-1 8 9-13h-6l1-7Z"></path>'], ['游戏规则','GAME RULES','管理游戏规则与相关功能选项。','<path d="M4 6h16M4 12h16M4 18h16"></path><circle cx="9" cy="6" r="2" fill="currentColor" stroke="none"></circle><circle cx="15" cy="12" r="2" fill="currentColor" stroke="none"></circle><circle cx="8" cy="18" r="2" fill="currentColor" stroke="none"></circle>'], ['实验性功能','EXPERIMENTAL','查看实验性选项及当前可用状态。','<path d="M9 3h6m-5 0v7l-6 9a1 1 0 0 0 1 2h14a1 1 0 0 0 1-2l-6-9V3M8 14h8"></path>'], ['调试','RUNTIME DIAGNOSTICS','只读探测平台改写，查看入口用途、实时状态与签名校验。','<path d="M8 3v3m8-3v3M4 10H1m22 0h-3M4 16H1m22 0h-3M8 21v-3m8 3v-3"></path><rect x="4" y="6" width="16" height="12" rx="3"></rect><path d="M9 10h6m-6 4h4"></path>'] ];
  let selectedCategory = 0, videoModule = 0, search = '', scheduled = false, renderedPage = '', toastTimer = null, capture = null, nextGroupId = 0, userNavigated = false, restoredNavigation = false;
  let debugGroup = '', debugFilter = '', debugGroupSignature = '';
  const debugRows = new Map();
  const groupNodes = new Map(), collapsed = new Set(), foldPreferencesSeen = new Set(), searchFolds = new Map();
  const bridge = global.chrome?.webview;
  const store = new ConsoleStore(message => { if (bridge?.postMessage) bridge.postMessage(message); }, schedule);
  global.RA2Console = Object.freeze({ store, receive: message => store.receive(message) });
  function schedule() { if (scheduled) return; scheduled = true; requestAnimationFrame(() => { scheduled = false; render(); }); }
  function notify(text) { const element = $('toast'); element.textContent = text; element.hidden = false; clearTimeout(toastTimer); toastTimer = setTimeout(() => { element.hidden = true; }, 6000); }
  function setSearch(value) { if(search!==value)searchFolds.clear();search=value; }
  function navigate(category, module = videoModule, restored = false) { if (!restored) userNavigated = true; selectedCategory = category; if (category === 0) videoModule = module; setSearch(''); $('search').value = ''; capture = null; if (restored) { store.current = {module: category === 0 ? videoModule : 2,category}; schedule(); } else store.navigate(category === 0 ? videoModule : 2, category); $('contentScroll').scrollTop = 0; }
  for (let index = 0; index < CATEGORIES.length; index++) { const button = document.createElement('button'); button.className = 'nav-button'; button.type = 'button'; button.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true">${CATEGORIES[index][3]}</svg><span></span>`; button.querySelector('span').textContent = CATEGORIES[index][0]; button.title = CATEGORIES[index][0]; button.addEventListener('click', () => navigate(index)); $('navigation').append(button); }
  $('videoTabs').addEventListener('click', event => { const button = event.target.closest('button[data-module]'); if (button) navigate(0, Number(button.dataset.module)); });
  $('videoTabs').addEventListener('keydown', event => {
    if (!['ArrowLeft','ArrowRight','Home','End'].includes(event.key)) return;
    event.preventDefault();
    const buttons = [...$('videoTabs').children], current = buttons.indexOf(document.activeElement);
    const index = event.key === 'Home' ? 0 : event.key === 'End' ? buttons.length - 1 : (Math.max(0,current) + (event.key === 'ArrowRight' ? 1 : buttons.length - 1)) % buttons.length;
    navigate(0,Number(buttons[index].dataset.module)); buttons[index].focus();
  });
  $('search').setAttribute('aria-keyshortcuts','Control+f');
  $('search').addEventListener('input', event => { setSearch(event.target.value); schedule(); });
  $('search').addEventListener('keydown', event => { if (event.key === 'Escape' && !capture) { event.preventDefault(); $('search').value = ''; setSearch(''); schedule(); } });
  $('clearSearch').addEventListener('click', () => { $('search').value = ''; setSearch(''); if(selectedCategory===5){debugGroup=debugFilter='';$('debugGroup').value=$('debugFilter').value='';} schedule(); $('search').focus(); });
  $('debugGroup').addEventListener('change',event=>{debugGroup=event.target.value;schedule();});
  $('debugFilter').addEventListener('change',event=>{debugFilter=event.target.value;schedule();});
  $('moduleToggle').addEventListener('change', event => store.enable(videoModule, event.target.checked));
  $('retrySave').addEventListener('click', () => store.retryConfig());
  $('foldAll').addEventListener('click', () => {
    const groups = [...groupNodes.values()].filter(group => group.hasDetails);
    const shouldCollapse = groups.some(group => !group.body.hidden);
    for (const group of groups) setGroupCollapsed(group,shouldCollapse);
    schedule();
  });
  document.addEventListener('keydown', event => { if (!capture && event.ctrlKey && event.key.toLowerCase() === 'f') { event.preventDefault(); $('search').focus(); $('search').select(); } });
  const observer = typeof IntersectionObserver === 'function' ? new IntersectionObserver(entries => { for (const entry of entries) { const group = entry.target._group; if (!group) continue; group.intersecting = entry.isIntersecting; if (entry.isIntersecting) updateGroup(group); else if (!group.body.contains(document.activeElement)) unmountGroup(group); } }, { root: $('contentScroll'), rootMargin: '600px' }) : null;
  function clearGroups() { for (const group of groupNodes.values()) { observer?.unobserve(group.section); releaseChunks(group); group.section.remove(); } groupNodes.clear(); }
  const estimateRows = controls => settingLines(controls).reduce((height,line) => height + (line.some(control=>control.type==='hotkey') ? 72 : 44),0);
  const chunkObserver = typeof IntersectionObserver === 'function' ? new IntersectionObserver(entries => { for (const entry of entries) { const chunk = entry.target._chunk; if (!chunk) continue; chunk.intersecting = entry.isIntersecting; if (entry.isIntersecting) updateChunk(chunk); else unmountChunk(chunk); } }, {root:$('contentScroll'),rootMargin:'500px'}) : null;
  // A search can reveal and fold matches without overwriting the page's saved folds.
  function setGroupCollapsed(group,value) {
    const key=`${renderedPage}/${group.name}`;
    if(group.searching){searchFolds.set(key,value);return;}
    foldPreferencesSeen.add(key);if(value)collapsed.add(key);else collapsed.delete(key);
  }
  function makeGroup(name) {
    const section=document.createElement('section'),summary=document.createElement('button'),title=document.createElement('span'),count=document.createElement('span'),header=document.createElement('div'),status=document.createElement('div'),body=document.createElement('div');
    section.className='settings-group';summary.className='group-summary';summary.type='button';title.className='group-title';title.textContent=name;count.className='group-count';header.className='group-header-controls';
    const arrow=document.createElementNS('http://www.w3.org/2000/svg','svg');arrow.setAttribute('viewBox','0 0 12 12');arrow.setAttribute('class','chevron');arrow.innerHTML='<path d="m3 4 3 3 3-3" fill="none" stroke="currentColor" stroke-width="1.5"/>';
    summary.append(title,count,arrow);body.className='group-body';body.id=`settings-group-${++nextGroupId}`;summary.setAttribute('aria-controls',body.id);
    // Keep current failures outside the body, so closing parameters never hides them.
    status.className='group-status';status.dataset.tone='error';status.hidden=true;status.setAttribute('role','alert');section.append(summary,header,body,status);
    const group={name,section,summary,count,header,status,headerRows:new Map(),body,controls:[],rows:new Map(),chunks:[],intersecting:!observer};section._group=group;
    summary.addEventListener('click',()=>{setGroupCollapsed(group,!group.body.hidden);updateGroup(group);updateFoldAction();});
    observer?.observe(section);return group;
  }
  function releaseChunks(group) { for (const chunk of group.chunks) { chunkObserver?.unobserve(chunk.element); chunk.element.remove(); } group.chunks=[]; group.chunkSignature=''; }
  function unmountGroup(group) { if (group.body.contains(document.activeElement)) return; const measured=group.body.getBoundingClientRect().height; group.body.style.minHeight = `${measured || estimateRows(group.controls)}px`; releaseChunks(group); group.body.replaceChildren();group.body._settingLines?.clear(); group.rows.clear(); }
  function unmountChunk(chunk) { if (chunk.element.contains(document.activeElement)) return; const measured=chunk.element.getBoundingClientRect().height; chunk.element.style.minHeight=`${measured || estimateRows(chunk.controls)}px`; chunk.element.replaceChildren();chunk.element._settingLines?.clear(); chunk.rows.clear(); }
  function updateRows(container,rows,controls,groupTitle='',subsettings=false) {
    const ids=new Set(controls.map(control=>control.id));
    for(const [id,row] of rows)if(!ids.has(id)){row.element.remove();rows.delete(id);}
    const lines=container._settingLines || (container._settingLines=new Map()), wanted=new Set();let after=null;
    for(const members of settingLines(controls)) {
      const first=members[0],key=first.row>0 && first.column>0?`row:${first.row}`:`id:${first.id}`;
      wanted.add(key);let line=lines.get(key);
      if(!line){line=document.createElement('div');line.className='settings-line';lines.set(key,line);}
      const compound=members.length>1,name=first.label || first.text;
      line.classList.toggle('compound-row',compound);line.classList.toggle('subsettings-line',subsettings);
      const hasPreset=members.some(control=>control.type==='choice' && control.options.includes('仅鼠标') && control.options.includes('自定义'));
      let previous=null;
      for(const [index,control] of members.entries()) {
        let row=rows.get(control.id);
        if(!row || row.type!==control.type || row.presentation!==control.presentation || Boolean(row.textInput)!==(control.type==='choice' && !control.readonly && control.presentation!=='list')) {
          const next=makeRow(control);if(row)row.element.replaceWith(next.element);row=next;rows.set(control.id,row);
        }
        row.control=control;
        row.element.dataset.settingLevel=control.type==='label'?'status':subsettings || (compound && index>0) || !['toggle','button'].includes(control.type)?'secondary':'primary';
        row.inlineLabel=compound && index>0 && control.label===name?inlineLabel(control,hasPreset):null;
        if(control.groupHeader && control.label===groupTitle)row.inlineLabel='';
        // A pushbutton already carries its action name. Do not print it twice.
        if(control.type==='button' && control.label===control.text)row.inlineLabel='';
        patchRow(row);placeAfter(line,row.element,previous);previous=row.element;
      }
      placeAfter(container,line,after);after=line;
    }
    for(const [key,line] of lines)if(!wanted.has(key)){line.remove();lines.delete(key);}
  }
  function updateChunk(chunk) { if (!chunk.intersecting && chunkObserver && !chunk.element.contains(document.activeElement)) { unmountChunk(chunk); return; } chunk.element.style.minHeight=''; updateRows(chunk.element,chunk.rows,chunk.controls,'',chunk.subsettings); }
  function updateGroup(group) {
    const key=`${renderedPage}/${group.name}`;
    if(!foldPreferencesSeen.has(key)){foldPreferencesSeen.add(key);if(group.controls.some(control=>control.collapsible&&control.collapsedDefault))collapsed.add(key);}
    const ackErrors=new Set(group.controls.filter(control=>store.error(control.id)).map(control=>control.id));
    const state=groupPresentation(group.controls,collapsed.has(key),group.searching,ackErrors,searchFolds.get(key));
    group.section.classList.toggle('collapsed',state.collapsed);group.section.classList.toggle('has-runtime-error',state.hasError);
    group.hasDetails=state.details.length>0;group.summary.disabled=!group.hasDetails;
    group.summary.querySelector('.chevron').toggleAttribute('hidden',!group.hasDetails);
    group.summary.setAttribute('aria-expanded',String(!state.collapsed));
    const countText=`${group.controls.length} 项${state.hasError?' · 需要处理':''}`;if(group.count.textContent!==countText)group.count.textContent=countText;
    const errors=state.hasError?group.controls.flatMap(control=>{
      const error=store.error(control.id),label=control.label || control.text || '设置';
      if(error)return [`${label}：${error}`];
      if(!control.runtimeError)return [];
      return [control.type==='label'?label:`${label}${control.text&&control.text!==label?'：'+control.text:'：运行异常'}`];
    }):[];
    const statusText=[...new Set(errors)].join('\n');if(group.status.textContent!==statusText)group.status.textContent=statusText;group.status.hidden=!state.hasError;
    updateRows(group.header,group.headerRows,state.headers,group.name);group.header.hidden=!state.headers.length;group.section.classList.toggle('has-header-controls',Boolean(state.headers.length));group.section.classList.toggle('multiple-header-rows',settingLines(state.headers).length>1);
    group.body.hidden=state.collapsed;if(state.collapsed)return;
    if(!group.intersecting&&observer&&!group.body.contains(document.activeElement)){unmountGroup(group);return;}
    group.body.style.minHeight='';const details=state.details;
    if(details.length>48&&chunkObserver){
      const signature=details.map(control=>`${control.id}:${control.row}:${control.column}`).join(',');
      const lines=settingLines(details),chunks=[];let batch=[];
      for(const line of lines){if(batch.length && batch.length+line.length>24){chunks.push(batch);batch=[];}batch.push(...line);}if(batch.length)chunks.push(batch);
      if(group.chunkSignature!==signature){releaseChunks(group);group.body.replaceChildren();group.rows.clear();group.body._settingLines?.clear();group.chunkSignature=signature;for(const controls of chunks){const element=document.createElement('div'),chunk={element,controls,rows:new Map(),intersecting:false,subsettings:Boolean(state.headers.length)};element.className='rows-chunk';element.style.minHeight=`${estimateRows(controls)}px`;element._chunk=chunk;group.chunks.push(chunk);group.body.append(element);chunkObserver.observe(element);}}
      group.chunks.forEach((chunk,index)=>{chunk.controls=chunks[index];chunk.subsettings=Boolean(state.headers.length);updateChunk(chunk);});
    }else{if(group.chunks.length){releaseChunks(group);group.body.replaceChildren();}updateRows(group.body,group.rows,details,'',Boolean(state.headers.length));}
  }
  function updateFoldAction() {
    const groups=[...groupNodes.values()].filter(group=>group.hasDetails);
    $('foldAll').hidden=!groups.length;
    $('foldAll').textContent=groups.every(group=>group.body.hidden)?'全部展开':'全部收起';
  }
  function makeRow(control) {
    const element = document.createElement('div'), copy = document.createElement('div'), label = document.createElement('label'), description = document.createElement('p'), editor = document.createElement('div'); element.className = 'setting-row'; copy.className = 'setting-copy'; label.className = 'setting-label'; description.className = 'setting-description'; editor.className = 'setting-editor'; copy.append(label,description); element.append(copy,editor);
    element.dataset.type=control.type;
    const row = { element, copy, label, description, editor, control, type: control.type, presentation: control.presentation, input: null };
    const send = (action,value) => { if (!store.act(row.control.id,action,value)) notify('此操作目前不可用，请等待设置同步。'); };
    if (control.type === 'toggle') { const toggle = document.createElement('label'), input = document.createElement('input'), track = document.createElement('span'), state = document.createElement('span'); toggle.className = 'toggle'; input.type = 'checkbox'; input.setAttribute('role','switch'); track.className = 'switch-track';track.setAttribute('aria-hidden','true');state.className='toggle-state';state.setAttribute('aria-hidden','true'); input.addEventListener('change',event => send('toggle',event.target.checked)); toggle.append(input,state,track); editor.append(toggle); row.input = input;row.toggleState=state; }
    else if (control.type === 'button') { const button = document.createElement('button'); button.className = 'setting-button'; button.type = 'button'; button.addEventListener('click',event => {if(row.control.immediate&&event.detail>1)return;send('press');}); button.addEventListener('keydown',event=>{if(row.control.immediate&&event.repeat&&['Enter',' '].includes(event.key)){event.preventDefault();event.stopPropagation();}}); editor.append(button); row.input = button; }
    else if (control.type === 'choice') {
      const select = document.createElement('select'); select.addEventListener('change',event => { if (Number(event.target.value) >= 0) send('select',Number(event.target.value)); });
      if (control.presentation === 'list') { select.className = 'list-field'; select.size = 5; select.addEventListener('dblclick',() => { if (select.selectedIndex >= 0) send('activate',select.selectedIndex); }); select.addEventListener('keydown',event => { if (event.key === 'Enter' && select.selectedIndex >= 0) { event.preventDefault(); send('activate',select.selectedIndex); } }); }
      editor.append(select); row.input = select;
      if (!control.readonly && control.presentation !== 'list') { const input = document.createElement('input'); input.className = 'field choice-text'; input.type = 'text'; input.setAttribute('aria-label',`${control.label}（自定义值）`); bindText(row,input); editor.prepend(input); row.textInput = input; select.style.minWidth = '55px'; select.style.maxWidth = '85px'; }
    }
    else if (control.type === 'text' || control.type === 'number') { const input = document.createElement('input'); input.className = 'field'; input.type = control.type === 'number' ? 'number' : 'text'; bindText(row,input); editor.append(input); row.input = input; }
    else if (control.type === 'range') { editor.classList.add('range-editor'); const input = document.createElement('input'), output = document.createElement('output'); input.type = 'range'; output.className = 'range-value'; input.addEventListener('input',event => { store.draft(row.control.id,Number(event.target.value)); output.textContent = event.target.value; }); input.addEventListener('change',event => { store.drafts.delete(controlKey(store.current.module,row.control.id)); send('range',Number(event.target.value)); }); editor.append(input,output); row.input = input; row.output = output; }
    else if (control.type === 'hotkey') { editor.classList.add('hotkey-editor'); const button = document.createElement('button'), mode = document.createElement('select'), double = document.createElement('label'), check = document.createElement('input'), clear = document.createElement('button'), special = document.createElement('select'); button.type = 'button'; button.className = 'hotkey-capture'; button.addEventListener('click',() => { if (capture?.row === row) stopCapture(); else startCapture(row); }); check.type = 'checkbox'; double.className = 'hotkey-double'; double.append(check,document.createTextNode('双击')); clear.type = 'button'; clear.className = 'icon-button'; clear.textContent = '×'; clear.title = '清除快捷键'; clear.setAttribute('aria-label','清除快捷键'); clear.addEventListener('click',() => send('hotkey',0)); mode.setAttribute('aria-label','触发方式'); mode.addEventListener('change',() => send('hotkey',(Number(store.value(row.control,'hotkey')) & ~0x3000) | Number(mode.value))); check.addEventListener('change',() => send('hotkey',(Number(store.value(row.control,'hotkey')) & ~0x8000) | (check.checked ? 0x8000 : 0))); special.setAttribute('aria-label','选择特殊按键'); special.addEventListener('change',() => { const vk = Number(special.value); if (vk) send('hotkey',(Number(store.value(row.control,'hotkey')) & 0xb700) | vk); special.value = '0'; }); editor.append(button,mode,double,clear,special); row.input = button; row.mode = mode; row.double = check; row.clear = clear; row.special = special; }
    else { element.classList.add('label-row'); editor.remove(); }
    const inputId = `control-${store.current.module}-${control.id}`; if (row.input) { row.input.id = inputId; label.htmlFor = inputId; } return row;
  }
  function bindText(row,input) { input.autocomplete = 'off'; input.spellcheck = false; input.addEventListener('focus',() => { store.beginEdit(row.control.id); store.draft(row.control.id,input.value); }); input.addEventListener('input',() => store.draft(row.control.id,input.value)); input.addEventListener('blur',() => store.commit(row.control.id)); input.addEventListener('keydown',event => { if (event.key === 'Enter') { event.preventDefault(); store.commit(row.control.id); input.blur(); } else if (event.key === 'Escape') { event.preventDefault(); store.cancelEdit(row.control.id); input.blur(); } }); }
  function patchRow(row) {
    const control = row.control, error = store.error(control.id); row.element.dataset.pending = String(store.isPending(control.id)); row.element.classList.toggle('is-disabled',!control.enabled); row.element.classList.toggle('has-error',Boolean(error)||control.runtimeError);
    let labelText=row.inlineLabel ?? (control.label || (control.type === 'label' ? control.text : '设置')); const hintText=error || (control.runtimeError&&control.type!=='label'?(control.text||control.label):''); if(control.type==='button' && labelText===(control.text || control.label || '执行'))labelText=''; if(row.label.textContent!==labelText) row.label.textContent=labelText;row.label.hidden=!labelText; if(row.description.textContent!==hintText) row.description.textContent=hintText; row.description.hidden = !row.description.textContent;row.copy.hidden=!labelText&&!hintText; row.element.title = control.hint;
    if (!row.input) return;
    const disabled = !control.enabled || store.closed || store.state?.generation!==store.generation.get(store.current.module);
    row.element.classList.toggle('is-disabled',disabled);
    row.input.disabled = disabled; row.input.setAttribute('aria-label',row.inlineLabel?`${control.label} · ${row.inlineLabel}`:control.label || control.text || '设置');
    if (control.type === 'toggle') { row.input.checked = Boolean(store.value(control,'toggle'));row.toggleState.textContent=row.input.checked?'已开启':'已关闭'; }
    else if (control.type === 'button') { row.input.textContent = control.text || control.label || '执行'; row.input.disabled = disabled || store.isPending(control.id); row.input.className=`setting-button action-${control.actionRole}`; }
    else if (control.type === 'choice') { const optionSignature = JSON.stringify([control.options,control.optionEnabled]); if (row.optionSignature !== optionSignature) { row.optionSignature = optionSignature; row.input.replaceChildren(...control.options.map((text,index) => { const option = document.createElement('option'); option.value = String(index); option.textContent = text; option.disabled = control.optionEnabled[index] === false; return option; })); } const index = Number(store.value(control,'select')); if (row.input.selectedIndex !== index) row.input.selectedIndex = index; if (row.textInput) { row.textInput.disabled = disabled; if (document.activeElement !== row.textInput) row.textInput.value = String(store.value(control,'text')); } }
    else if (control.type === 'text' || control.type === 'number') { row.input.readOnly = control.readonly; if (document.activeElement !== row.input || control.readonly) row.input.value = String(store.value(control,'text')); }
    else if (control.type === 'range') { row.input.min = control.min; row.input.max = control.max; row.input.step = control.step; const value = store.value(control,'range'); if (document.activeElement !== row.input) row.input.value = value; row.output.textContent = String(value); }
    else if (control.type === 'hotkey') { const value = Math.max(0,Number(store.value(control,'hotkey')) || 0), modes = control.options.filter(option => ['press','release','hold'].includes(option)); const allowed = modes.length ? modes : ['press','release','hold']; const signature = allowed.join(','); if (signature !== row.modeSignature) { row.modeSignature = signature; row.mode.replaceChildren(...allowed.map(name => { const option = document.createElement('option'); option.value = String(MODES[name]); option.textContent = {press:'按下触发',release:'松开触发',hold:'持续按住'}[name]; return option; })); }row.mode.hidden=allowed.length===1; const keyboardOnly = control.options.includes('keyboardOnly'); if (row.keyboardOnly !== keyboardOnly) { row.keyboardOnly = keyboardOnly; row.special.replaceChildren(...[['特殊按键…',0],['Esc',27],['Enter',13],['Tab',9],...(!keyboardOnly ? [['鼠标左键',1],['鼠标右键',2],['鼠标中键',4],['鼠标侧键 1',5],['鼠标侧键 2',6]] : [])].map(([text,vk]) => { const option = document.createElement('option'); option.textContent = text; option.value = String(vk); return option; })); } const mode = value & 0x3000 || (allowed.includes('hold') && allowed.length === 1 ? MODES.hold : MODES.press); row.mode.value = String(mode); row.double.checked = Boolean(value & 0x8000); row.mode.disabled = row.double.disabled = row.clear.disabled = disabled || !(value & 255); row.special.disabled = disabled; row.input.textContent = capture?.row === row ? '请按下组合键…' : keyName(value); row.input.classList.toggle('capturing',capture?.row === row); }
  }
  function startCapture(row) { stopCapture(); capture = { row, modifier: null, usedModifier: false }; row.input.focus(); patchRow(row); }
  function stopCapture() { const previous = capture?.row; capture = null; if (previous) patchRow(previous); }
  document.addEventListener('keydown',event => { if (!capture) return; event.preventDefault(); event.stopPropagation(); if (event.repeat) return; if (event.key === 'Escape') { stopCapture(); return; } if (event.metaKey) { notify('当前快捷键格式不支持 Win 组合键，请使用 Ctrl、Shift 或 Alt。'); return; } if (['Shift','Control','Alt'].includes(event.key)) { capture.modifier = event; return; } capture.usedModifier = true; const row = capture.row, previous = Number(store.value(row.control,'hotkey')) || 0, value = encodeHotkey(event,previous); if (value !== null) { stopCapture(); store.act(row.control.id,'hotkey',value); } },true);
  document.addEventListener('keyup',event => { if (!capture || capture.usedModifier || !capture.modifier || event.key !== capture.modifier.key) return; event.preventDefault(); const row = capture.row, value = encodeHotkey(capture.modifier,Number(store.value(row.control,'hotkey')) || 0); if (value !== null) { stopCapture(); store.act(row.control.id,'hotkey',value); } },true);
  document.addEventListener('pointerdown',event => { if (capture && event.target !== capture.row.input) stopCapture(); },true);
  document.addEventListener('focusin',event => { if (capture && event.target !== capture.row.input) stopCapture(); });
  global.addEventListener('blur',stopCapture);
  function makeDebugRow() {
    const element=document.createElement('div'),entry=document.createElement('div'),purposeCell=document.createElement('div'),stateCell=document.createElement('div'),toggleCell=document.createElement('div');
    element.className='debug-row';element.setAttribute('role','row');
    for(const cell of [entry,purposeCell,stateCell,toggleCell])cell.setAttribute('role','cell');
    purposeCell.className='debug-purpose-cell';stateCell.className='debug-state-cell';toggleCell.className='debug-toggle-cell';
    const label=document.createElement('strong'),address=document.createElement('code'),meta=document.createElement('p'),purpose=document.createElement('p'),features=document.createElement('p');
    label.className='debug-entry-name';address.className='debug-address';meta.className='debug-entry-meta';purpose.className='debug-purpose';features.className='debug-features';
    entry.append(label,address,meta);purposeCell.append(purpose,features);
    const badges=document.createElement('div'),status=document.createElement('span'),hook=document.createElement('span'),detail=document.createElement('p'),age=document.createElement('p');
    badges.className='debug-badges';status.className=hook.className='debug-badge';detail.className='debug-detail';age.className='debug-age';badges.append(status,hook);stateCell.append(badges,detail,age);
    const caption=document.createElement('span'),toggle=document.createElement('label'),input=document.createElement('input'),track=document.createElement('span'),toggleState=document.createElement('p'),error=document.createElement('p');
    caption.className='debug-toggle-caption';caption.textContent='校验签名';toggle.className='toggle';input.type='checkbox';input.setAttribute('role','switch');track.className='switch-track';track.setAttribute('aria-hidden','true');toggle.append(input,track);
    toggleState.className='debug-toggle-state';error.className='debug-action-error';error.setAttribute('role','alert');toggleCell.append(caption,toggle,toggleState,error);element.append(entry,purposeCell,stateCell,toggleCell);
    const row={element,label,address,meta,purpose,features,status,hook,detail,age,input,toggleState,error,control:null};
    input.addEventListener('change',()=>{if(row.control)store.act(row.control.id,'toggle',input.checked);schedule();});
    return row;
  }
  function renderSettingsGroups(controls,query='') {
    const groups=groupControls(controls,query),count=[...groups.values()].reduce((sum,items)=>sum+items.length,0);
    for(const [name,group]of groupNodes)if(!groups.has(name)){observer?.unobserve(group.section);releaseChunks(group);group.section.remove();groupNodes.delete(name);}
    let after=null;for(const [name,items]of groups){let group=groupNodes.get(name);if(!group){group=makeGroup(name);groupNodes.set(name,group);}group.controls=items;group.searching=Boolean(query.trim());placeAfter($('settings'),group.section,after);after=group.section;updateGroup(group);}
    return {groups,count};
  }
  function renderDebug(state) {
    // Diagnostics actions are ordinary adapter-issued controls. Keep them
    // reachable even when the catalogue search/filter has no matching entry.
    const tools=debugToolControls(state.controls);renderSettingsGroups(tools);$('settings').hidden=!tools.length;
    const now=Date.now(),current=state.generation===store.generation.get(state.module),all=state.controls.filter(control=>control.visible && control.debug?.present);
    const groups=[...new Set(all.map(control=>control.group || '未分组'))],signature=JSON.stringify(groups);
    if(debugGroupSignature!==signature){debugGroupSignature=signature;$('debugGroup').replaceChildren(...['',...groups].map(name=>{const option=document.createElement('option');option.value=name;option.textContent=name || '全部分组';return option;}));if(!groups.includes(debugGroup))debugGroup='';$('debugGroup').value=debugGroup;}
    const controls=filterDebugControls(all,{search,group:debugGroup,state:debugFilter,receivedAt:state.receivedAt,now,current}),visible=new Set(controls.map(control=>control.id));
    for(const [id,row] of debugRows)if(!visible.has(id)){row.element.remove();debugRows.delete(id);}
    let after=null;
    for(const control of controls){
      let row=debugRows.get(control.id);if(!row){row=makeDebugRow();debugRows.set(control.id,row);}row.control=control;row.element.dataset.controlId=String(control.id);row.element.dataset.siteId=String(control.debug.siteId);
      const d=control.debug,status=debugStatus(control,state.receivedAt,now,current),pending=store.isPending(control.id),error=store.error(control.id);
      row.label.textContent=control.label || `入口 ${d.siteId}`;row.address.textContent=debugAddress(d.address);
      row.meta.textContent=[control.group || '未分组',debugKindLabel(d.kind)].filter(Boolean).join(' · ');
      row.purpose.textContent=d.purpose || '用途待补充';row.features.textContent=d.features?`影响：${d.features}`:'影响功能待补充';
      row.status.textContent=status.label;row.status.dataset.tone=status.tone;row.hook.textContent=status.hookLabel;row.hook.dataset.tone=status.hook==='conflict'?'conflict':status.hook==='pending'?'pending':'neutral';
      row.detail.textContent=d.detail || (status.sampled?'':'等待游戏内观测');row.detail.hidden=!row.detail.textContent;
      row.age.textContent=!current?'正在等待当前页面快照':!status.sampled?'尚无运行时观测':status.ageMs<1000?'刚刚观测':`距上次观测 ${Math.floor(status.ageMs/1000)} 秒`;
      row.input.setAttribute('aria-label',`校验签名 · ${control.label || d.siteId} (${debugAddress(d.address)})`);row.input.disabled=store.closed || !current || !control.enabled || control.readonly || control.type!=='toggle';row.input.checked=Boolean(store.value(control,'toggle'));
      row.toggleState.textContent=debugToggleLabel(control,pending);row.toggleState.dataset.off=String(d.kind!=='system' && d.state!=='unused' && !control.checked && !pending);row.error.textContent=error;row.error.hidden=!error;row.element.classList.toggle('has-action-error',Boolean(error));
      placeAfter($('debugList'),row.element,after);after=row.element;
    }
    const blocked=all.filter(control=>debugStatus(control,state.receivedAt,now,current).state==='blocked').length,unknown=all.filter(control=>debugStatus(control,state.receivedAt,now,current).state==='unknown').length;
    $('resultCount').textContent=`${controls.length} / ${all.length} 个入口 · ${blocked} 项受阻 · ${unknown} 项待观测`;
    $('emptyState').hidden=controls.length>0;$('debugPanel').hidden=controls.length===0;$('foldAll').hidden=true;
  }
  function render() {
    const category = CATEGORIES[selectedCategory], state = store.state, currentPage = pageKey(store.current.module,store.current.category);
    if (renderedPage !== currentPage) { clearGroups();searchFolds.clear(); for(const row of debugRows.values())row.element.remove();debugRows.clear(); renderedPage = currentPage; }
    [...$('navigation').children].forEach((button,index) => { if (index === selectedCategory) button.setAttribute('aria-current','page'); else button.removeAttribute('aria-current'); });
    $('breadcrumb').textContent = category[0]; $('pageTitle').textContent = category[0]; $('eyebrow').textContent = category[1]; $('pageDescription').textContent = category[2]; $('version').textContent = store.shell.version || '正在连接…';
    $('videoTabs').hidden = selectedCategory !== 0; $('categoryNote').hidden = selectedCategory === 0;
    const debugging=selectedCategory===5;$('settings').hidden=!state;$('debugTools').hidden=!debugging;$('debugPanel').hidden=!debugging || !state;$('categoryNote').textContent=debugging?'平台探测与入口状态':'每项功能独立启用';$('search').placeholder=debugging?'搜索入口、地址或影响功能…':'搜索当前页面…';$('contentScroll').setAttribute('aria-label',debugging?'平台 Hook 探测与入口调试':'功能设置');
    $('emptyState').querySelector('h2').textContent=debugging?'没有匹配的入口':'没有匹配的设置';$('clearSearch').textContent=debugging?'清除搜索与筛选':'清除搜索';
    [...$('videoTabs').children].forEach(button => { const selected=Number(button.dataset.module) === videoModule;button.setAttribute('aria-selected',String(selected));button.tabIndex=selected?0:-1;button.setAttribute('aria-controls','settings'); });
    const moduleState = store.shell.modules.find(module => module.module === videoModule); $('moduleToggleWrap').hidden = selectedCategory !== 0 || !moduleState; if (moduleState) { $('moduleToggle').checked = Boolean(moduleState.enabled); $('moduleToggle').disabled = store.closed || !moduleState.ready; $('moduleToggleLabel').textContent=moduleState.enabled?'模块已启用':'启用此模块'; }
    const serviceReady=store.connected && store.shell.servicesReady && store.shell.modules.some(module=>module.ready); $('connectionDot').classList.toggle('ready',serviceReady && !store.closed); $('connectionText').textContent = store.closed ? '正在退出' : serviceReady ? '控制台就绪' : store.connected ? '模块初始化中' : '正在连接';
    const config = store.shell.config || {};
    $('retrySave').hidden=!config.failed; $('retrySave').disabled=!config.canRetry || Boolean(store.configRetryPending);
    $('retrySave').textContent=store.configRetryPending?'正在请求…':config.pending || config.busy?'保存中…':'重试保存';
    $('saveState').textContent = config.failed ? '保存失败' : store.pending.size ? '正在应用设置…' : config.pending || config.busy ? '保存中…' : store.drafts.size ? '正在编辑' : store.shell.config ? '已保存' : '设置自动保存'; $('pendingCount').textContent = store.pending.size ? `${store.pending.size} 项等待确认` : '';
    $('saveState').dataset.state=config.failed?'error':store.pending.size||config.pending||config.busy?'busy':store.drafts.size?'editing':store.shell.config?'saved':'idle';
    const globalError = String(store.shell.error || (config.failed ? config.error || '配置尚未保存，请重试或查看保存错误详情。' : '') || store.notice);
    $('globalNotice').textContent = globalError; $('globalNotice').hidden = !globalError;
    $('statusText').textContent = store.closed ? '正在关闭模块并保存设置…' : globalError || state?.status || (store.connected ? '操作会自动保存，独立功能可按需启用。' : '界面已就绪，正在等待模块状态。');
    $('loadingState').hidden = Boolean(state); $('loadingText').textContent = !bridge ? '此页面需要由红警工具箱打开。' : '正在等待功能模块状态…';
    if(!config.failed) render.lastConfigError='';
    if (config.failed && config.error && config.error !== render.lastConfigError) { render.lastConfigError = config.error; notify(config.error); }
    if (!state) { $('resultCount').textContent = debugging?'正在载入入口目录':'正在载入设置'; $('emptyState').hidden = true; $('foldAll').hidden = true; return; }
    if(debugging)renderDebug(state);else {
    const {groups,count}=renderSettingsGroups(state.controls,search);
    $('resultCount').textContent = search ? `找到 ${count} 项设置` : `${count} 项设置 · ${groups.size} 个分组`; $('emptyState').hidden = count > 0;
    updateFoldAction();
    }
    if (store.errors.size) { const latest = [...store.errors.values()].at(-1); if (latest !== render.lastError) { render.lastError = latest; notify(latest); } }

  }
  bridge?.addEventListener('message',event => { let message = event.data; if (typeof message === 'string') { try { message = JSON.parse(message); } catch { return; } } if (!store.receive(message)) return; if (message.kind === 'shell' && message.servicesReady && !restoredNavigation) { restoredNavigation = true; if (!userNavigated) { const category = Number.isInteger(message.selectedCategory) ? Math.max(0,Math.min(5,message.selectedCategory)) : 0, module = message.videoSection === 1 ? 1 : 0; videoModule = module; navigate(category,module,true); } } });
  global.addEventListener('beforeunload',() => stopCapture());
  // Relative sample age keeps advancing if the owner/game stops publishing.
  global.setInterval(()=>{if(selectedCategory===5 && store.state)schedule();},1000);
  store.ready(); schedule();
})(globalThis);
