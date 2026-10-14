// Online multiplayer front-end screens (web/ui.js delegates screens it does not own, like career-ui.js):
//   mp-connect  connecting / server unreachable / version mismatch
//   mp-lobbies  lobby browser: Create Lobby, every open lobby, Refresh, Back
//   mp-lobby    the lobby: Ready, Start Race (host), Course (host), Copy Invite Link, Chat, Leave Lobby
//   mp-card     pv mpPreRace: the race's card over the gate idle (overlay 8 '40race_pre', as a Single Event's), Continue = ready;
//               then 'Waiting for other player...' (0x258CB0's kT_OVRCMNWaitPlayer) until the server's GO
//   results     during an online race (onlineResults): the original Single Event Results screen (0x23A760 layout,
//               as ui.js draws it) with every online racer: recorded finish times, 0x122D78 estimates for racers
//               still on course, DNF last. It keeps ui.js's 'results' screen name, so main.js keeps drawing the
//               finish camera behind it exactly as in a single event; ui.showResults lands here unchanged.
// Lobby screens are drawn with the career menus' frame/help helpers so they match the rest of the front end.
import { inviteLink } from './net/mp-client.js';
import { raceTime } from './race-time.mjs';
import { pv } from './pv-flags.js';
const Y = (y) => Math.round(y * 448 / 480);
const SCREENS = ['mp-connect', 'mp-lobbies', 'mp-lobby', 'mp-card'];
const ORDINAL = ['1st', '2nd', '3rd', '4th', '5th', '6th'];

export class MultiplayerScreens {
  constructor(ui, session) {
    this.ui = ui;
    this.session = session;
    this.notice = '';
    this.noticeUntil = 0;
    this.onlineResults = false;
    // pv onlineTimeAttack: the time-attack screens (web/ta-ui.js, loaded only with the switch on), a 'Time Attack' row above the
    // lobbies once they are loaded
    this.ta = null;
    if (pv('onlineTimeAttack') && ui.timeAttack) {
      import('./ta-ui.js')
        .then(({ TaScreens }) => {
          this.ta = new TaScreens(ui, ui.timeAttack);
          return this.ta.load();
        })
        .then(() => ui.sync?.())
        .catch((e) => console.warn('Time attack screens unavailable', e));
    }
  }
  // the lobby list's first row is Time Attack while its screens are ready
  get taRow() {
    return this.ta?.ready ? 1 : 0;
  }
  get client() {
    return this.session.client;
  }
  get racing() {
    return this.session.racing;
  }
  owns(screen) {
    return SCREENS.includes(screen) || (screen === 'results' && this.onlineResults) || !!this.ta?.owns(screen);
  }
  // Main menu -> Online -> rider select -> here.
  enter() {
    this.ui.set('mp-connect');
    this.session
      .connect()
      .then(() => {
        if (this.client.state.status === 'online') this.ui.set(this.client.state.lobby ? 'mp-lobby' : 'mp-lobbies');
        else this.ui.sync();
      })
      .catch(() => this.ui.sync());
  }
  flash(text, ms = 2500) {
    this.notice = text;
    this.noticeUntil = performance.now() + ms;
  }
  me() {
    const s = this.client.state;
    return s.lobby?.members.find((m) => m.id === s.id);
  }
  items(screen) {
    if (this.ta?.owns(screen)) return this.ta.items(screen);
    const s = this.client.state;
    if (screen === 'mp-connect') return [s.status === 'connecting' || s.status === 'reconnecting' ? 'Connecting...' : 'Retry', 'Back'];
    if (screen === 'mp-lobbies')
      return [
        ...(this.taRow ? ['Time Attack'] : []),
        'Create Lobby',
        ...s.lobbies.map((l) => `${l.name} (${l.players}/${l.maxPlayers})${l.racing ? ' racing' : ''}`),
        'Refresh',
        'Back'
      ];
    if (screen === 'mp-lobby')
      return [
        this.me()?.ready ? 'Not Ready' : 'Ready',
        this.client.isHost ? 'Start Race' : 'Waiting for host',
        `Course: ${this.courseName(s.lobby?.course)}`,
        'Copy Invite Link',
        'Chat',
        'Leave Lobby'
      ];
    if (screen === 'results') return ['Back to Lobby'];
    if (screen === 'mp-card') return [this.cardLabel()];
    return [];
  }
  // the card's one item: Continue (0x0F3AB955), then the PS2's 'Waiting for other player...' (0x0BA409E2)
  cardLabel() {
    const t = (key, fallback) => this.ui.careerUI?.t?.(key, fallback) || fallback;
    return this.session.card?.ready ? t(0x0ba409e2, 'Waiting for other player...') : t(0x0f3ab955, 'Continue');
  }
  disabled(screen, i) {
    if (this.ta?.owns(screen)) return this.ta.disabled(screen, i);
    const s = this.client.state;
    if (screen === 'mp-connect') return i === 0 && (s.status === 'connecting' || s.status === 'reconnecting');
    if (screen === 'mp-lobbies') {
      const l = s.lobbies[i - 1 - this.taRow];
      return !!l && l.players >= l.maxPlayers;
    }
    if (screen === 'mp-lobby') return (i === 1 || i === 2) && (!this.client.isHost || !!s.lobby?.race);
    if (screen === 'mp-card') return !!this.session.card?.ready;
    return false;
  }
  layout(screen, i) {
    if (this.ta?.owns(screen)) return this.ta.layout(screen, i);
    if (screen === 'mp-lobby') return [40, Y(120) + i * Y(30), 250, Y(26)];
    if (screen === 'results') return [450, 299 + i * 15, 126, 17];
    if (screen === 'mp-card') return [430, Y(380), 160, Y(24)];
    return [40, Y(110) + i * Y(26), 360, Y(24)];
  }
  key(e) {
    return !!this.ta?.key(e);
  }
  // the in-race HUD line of a time-attack session (web/ui.js after the career HUD)
  hud(c, s) {
    this.ta?.hud(c, s);
  }
  // over a cutscene: the time-attack podium's labels (web/ui.js after the cutscene's own overlay)
  cutsceneOverlay(c) {
    this.ta?.cutsceneOverlay(c);
  }
  choose(i) {
    const ui = this.ui,
      s = this.client.state;
    if (this.ta?.owns(ui.screen)) {
      this.ta.choose(i);
      return;
    }
    if (ui.screen === 'mp-lobbies' && this.taRow) {
      if (i === 0) {
        this.ta.open();
        return;
      }
      i--;
    }
    if (ui.screen === 'mp-connect') {
      if (i === 0) this.enter();
      else this.leaveToMenu();
      return;
    }
    if (ui.screen === 'mp-lobbies') {
      const lobbies = s.lobbies;
      if (i === 0) {
        // pv onlineTimeAttack: the Create Lobby popup (web/ta-ui.js ta-create): the mode (Race / Time Attack) and its options
        if (this.taRow) this.ta.openCreate('race', 'mp-lobbies');
        else this.session.createLobby();
        return;
      }
      if (i === lobbies.length + 1) {
        this.client.list();
        this.flash('Lobby list refreshed');
        return;
      }
      if (i === lobbies.length + 2) {
        this.leaveToMenu();
        return;
      }
      const l = lobbies[i - 1];
      if (l && !this.disabled('mp-lobbies', i)) this.session.joinLobby(l.id);
      return;
    }
    if (ui.screen === 'mp-lobby') {
      if (i === 0) this.client.ready(!this.me()?.ready);
      else if (i === 1 && this.client.isHost && !s.lobby?.race) this.client.start();
      else if (i === 2 && this.client.isHost && !s.lobby?.race) this.chooseCourse();
      else if (i === 3) this.copyInvite();
      else if (i === 4) this.chat();
      else if (i === 5) {
        this.client.leave();
        ui.set('mp-lobbies');
      }
      return;
    }
    if (ui.screen === 'results') this.toLobby();
    // the card takes Continue from its phase 5 (web/screen-phases.js), as the Single Event card does
    if (ui.screen === 'mp-card' && (!ui.phases || ui.phases.accepts())) this.session.continueCard?.();
  }
  back() {
    const ui = this.ui;
    // the card has no Back (Triangle does nothing on overlay 8)
    if (ui.screen === 'mp-card') return;
    if (this.ta?.owns(ui.screen)) {
      this.ta.back();
      return;
    }
    if (ui.screen === 'mp-lobby') {
      this.client.leave();
      ui.set('mp-lobbies');
    } else if (ui.screen === 'results') this.toLobby();
    else this.leaveToMenu();
  }
  leaveToMenu() {
    this.session.disconnect();
    this.ui.onlineMode = false;
    this.ui.set('main');
  }
  // Pause > Quit during an online race: the racer is DNF and returns to the lobby.
  quitRace() {
    this.session.quitRace();
    this.onlineResults = false;
    this.ui.set(this.client.state.lobby ? 'mp-lobby' : 'mp-lobbies');
  }
  // Back from the results: the finish camera behind them stops (the local run ends, as Quit does) and the lobby opens.
  toLobby() {
    this.ui.cb.quit?.();
    this.session.backToLobby();
    this.onlineResults = false;
    this.ui.set(this.client.state.lobby ? 'mp-lobby' : 'mp-lobbies');
  }
  // The server's final order (every racer finished, DNF or out of time) while this racer is still on course.
  showResults() {
    this.onlineResults = true;
    if (this.ui.screen !== 'results') {
      this.ui.set('results');
      this.ui.index = 0;
      this.ui.sync();
    }
  }
  // The host's Course row. pv mpCourseSelect: the Single Event selector (Select Peak / Mode / Event, web/fe-event-select.js openLobby) picks
  // the lobby's course; nothing loads until the race starts. Off: the next course, loaded in the lobby by every page.
  chooseCourse() {
    const ui = this.ui,
      select = ui.eventSelect;
    if (!pv('mpCourseSelect') || !select?.ready || !select.openLobby) {
      this.session.cycleCourse();
      return;
    }
    const toLobby = () => {
      ui.set(this.client.state.lobby ? 'mp-lobby' : 'mp-lobbies');
      if (this.client.state.lobby) {
        ui.index = 2;
        ui.sync();
      }
    };
    select.openLobby({
      code: this.client.state.lobby?.course ?? null,
      pick: (e) => {
        this.session.chooseCourse?.(e.code);
        toLobby();
      },
      back: toLobby
    });
  }
  async copyInvite() {
    const id = this.client.state.lobby?.id;
    if (!id) return;
    const link = inviteLink(id);
    try {
      await navigator.clipboard.writeText(link);
      this.flash('Invite link copied');
    } catch {
      this.flash(link, 8000);
    }
  }
  chat() {
    const text = typeof prompt === 'function' ? prompt('Say to the lobby:') : '';
    if (text) this.client.chat(text);
  }
  courseName(code) {
    return (this.ui.courses || []).find((c) => c.code === code)?.name ?? code ?? '';
  }
  rows() {
    const raceTicks = this.ui.lastState?.raceTicks ?? 0;
    return this.session.resultRows(raceTicks);
  }
  draw(c, b) {
    const ui = this.ui,
      s = this.client.state,
      screen = ui.screen,
      career = ui.careerUI;
    if (screen === 'results') return this.drawResults(c);
    if (screen === 'mp-card') return this.drawCard(c);
    if (this.ta?.owns(screen)) return this.ta.draw(c, b);
    const frame = (title) => {
      if (career?.mcommFrame) career.mcommFrame(c, b, title);
      else {
        b.fillStyle = '#5d8aa9';
        b.fillRect(0, 0, 640, 448);
        ui.text(c, title, 106, Y(52), 24, '#eef4f7');
      }
    };
    const help = (text, buttons) => (career?.help ? career.help(c, text, buttons) : ui.text(c, text, 44, Y(396), 14, '#0c1a26'));
    const row = (text, x, y, size = 17, color = '#0f2533', font) => ui.text(c, text, x, y, size, color, font);
    const itemColor = (i) => (ui.index === i ? '#f4f6f2' : this.disabled(screen, i) ? '#51708a' : '#0f2533');
    if (screen === 'mp-connect') {
      frame('Online');
      const message =
        s.status === 'connecting'
          ? 'Connecting to the multiplayer server...'
          : s.status === 'reconnecting'
            ? 'Reconnecting...'
            : s.error || 'Not connected';
      ui.wrap(message, 520, 18)
        .slice(0, 3)
        .forEach((line, k) => row(line, 60, Y(140) + k * Y(24), 18, '#f4f6f2'));
      ui.items().forEach((t, i) => row(t, 60, Y(220) + i * Y(30), 20, itemColor(i)));
      help(
        s.status === 'version'
          ? 'The server runs a different version of the game: reload the page.'
          : 'Start the server with: cd web && npm run online. Friends on your network open the link it prints.'
      );
      return;
    }
    if (screen === 'mp-lobbies') {
      frame('Online Lobbies');
      b.fillStyle = 'rgba(242,245,246,.85)';
      b.fillRect(30, Y(96), 400, Y(270));
      ui.items().forEach((t, i) => row(t, 46, Y(114) + i * Y(26), 18, itemColor(i)));
      if (!s.lobbies.length) row('No lobbies yet: create one and send the invite link.', 46, Y(114) + 3 * Y(26), 14, '#355a74');
      row(`${s.status === 'online' ? 'Online' : s.status}${Number.isFinite(s.rtt) ? `  ${s.rtt} ms` : ''}`, 460, Y(110), 14, '#eef4f7');
      help(
        this.noticeText() ||
          (this.taRow && ui.index === 0
            ? 'Time attack sessions: everyone rides the same event against the clock; the best run wins.'
            : ui.index === this.taRow && this.taRow
            ? 'Create a race lobby or a time attack session: pick the mode, the event and its options.'
            : ui.index === this.taRow
            ? 'Create a lobby on the current course. Share its invite link with a friend.'
            : 'Join a lobby, or refresh the list. A racing lobby seats you for its next race.')
      );
      return;
    }
    if (screen === 'mp-lobby') {
      const l = s.lobby;
      frame(l ? l.name : 'Lobby');
      b.fillStyle = 'rgba(242,245,246,.85)';
      b.fillRect(30, Y(106), 270, Y(196));
      b.fillRect(320, Y(106), 290, Y(260));
      ui.items().forEach((t, i) => row(t, 46, Y(124) + i * Y(30), 18, itemColor(i)));
      row(`Lobby ${l?.id ?? ''}  -  ${this.courseName(l?.course)}`, 332, Y(124), 15, '#0f2533');
      (l?.members ?? []).forEach((m, i) => {
        const y = Y(156) + i * Y(28);
        // The host's name in the highlight colour (the front-end font has no star glyph).
        row(
          `${m.name}${m.id === s.id ? ' (you)' : ''}`.slice(0, 16),
          332,
          y,
          16,
          m.online === false ? '#8aa2b4' : m.id === l.hostId ? '#c46b04' : '#0f2533'
        );
        row(m.rider ? (m.rider[0].toUpperCase() + m.rider.slice(1)).slice(0, 9) : '', 468, y, 14, '#355a74');
        const status = m.online === false ? 'Away' : m.waiting ? 'Next' : l.race ? 'Racing' : m.ready ? 'Ready' : '-';
        row(status, 556, y, 14, m.ready || l.race ? '#2d7a2d' : '#51708a');
      });
      if (l?.race)
        row(
          `Race in progress: ${l.race.finished.length}/${l.members.filter((m) => !m.waiting).length} finished`,
          332,
          Y(156) + 6 * Y(28),
          14,
          '#355a74'
        );
      s.chat.slice(-4).forEach((m, k) => row(`${m.from}: ${m.text}`.slice(0, 44), 40, Y(318) + k * Y(18), 13, '#eef4f7'));
      help(
        this.noticeText() ||
          (l?.race
            ? 'A race is on. You are seated for the next one.'
            : this.client.isHost
              ? 'Start the race when everyone is in. Copy the invite link and send it to a friend.'
              : 'Waiting for the host to start. Set Ready when you are set.')
      );
    }
  }
  // pv mpPreRace: the race card over the gate idle: the Single Event card's own drawing (career-ui.js drawObjectivesOpen / 40race_pre,
  // web/results-lui.js raceCard): the event, the objective of the event's final round (0x23A174: a Single Event is round 3), the
  // racers in slot order (this page's in the human colour), Continue. The subtitle is 'Online' (0x0012F325): which line the PS2's
  // online card prints there is not traced.
  drawCard(c) {
    const ui = this.ui,
      cu = ui.careerUI;
    ui.cutscene?.drawFade?.(c);
    // after Continue the card is closed (Cross closes overlay 8: command 1). The waiting line is the HUD's: 0x258CB0 formats
    // kT_OVRCMNWaitPlayer and sends it to the 'hud' screen as command 14 (0x1EC2A0: the text at +0xA48, flag 0x40000), which
    // cOVStateHUD1P_onRender2D (0x1EC810) draws with 0x1F2DB0 in FEFONT (+0x428) at descriptor 63: centred on (320, 270), scale 0.7,
    // yellow, wrapped at 360 (the line fits on one); command 15 clears it.
    if (this.session.card?.ready) {
      const hud = ui.trickHud,
        list = [];
      if (hud && ui.trickHudRenderer && hud.desc(63)) {
        hud.text(list, 'FEFONT', 63, this.cardLabel());
        ui.trickHudRenderer.render(c, list);
      } else ui.text(c, this.cardLabel(), 320, Y(270), 16, '#f4f400', 'FEFONT', 'center');
      return;
    }
    const t = cu?.cardTicks ? cu.cardTicks() : 60;
    if (t < 3) return;
    if (t < 33 && cu?.luiPanels?.()) {
      cu.resultsLui.panel(c, '', '', { openOnly: true });
      return;
    }
    const code = ui.course?.code,
      index = cu?.data?.courses?.findIndex((x) => x.code === code) ?? -1;
    const mode = index >= 0 && cu?.career ? cu.career.standardMode(index) : 0;
    const ev = { mode, course: index, round: 3, career: false };
    let title = ui.course?.label || ui.course?.name || '',
      objective = '';
    try {
      if (index >= 0) title = cu.eventTitle(ev);
      if (index >= 0 && mode < 4)
        objective = cu.career
          .objectives(ev)
          .map((o) => (typeof o === 'string' ? cu.t(o) : cu.t(o.key)))
          .join('  ');
    } catch {}
    const race = this.session.race,
      riders = [...(race?.players ?? [])]
        .sort((a, b) => a.slot - b.slot)
        .map((p) => ({ name: p.name, human: p.slot === race.slot }));
    const label = this.cardLabel();
    if (
      cu?.resultsLui?.raceCard?.(c, {
        title,
        sub: cu.t(0x0012f325, 'Online'),
        objective,
        riders,
        recordLabel: '',
        record: null,
        continueLabel: label
      })
    )
      return;
    c.fillStyle = 'rgba(13,58,79,.92)';
    c.fillRect(50, Y(62), 540, Y(350));
    ui.text(c, title, 88, Y(96), 21, '#d1e1e2');
    riders.forEach((r, i) => ui.text(c, r.name, 120, Y(170) + i * Y(22), 17, r.human ? '#e8bd72' : '#d1e1e2'));
    ui.text(c, label, 450, Y(380), 15, '#dce5e4');
  }
  // The owned Single Event Results layout (ui.js 'results'), rows from the online race.
  drawResults(c) {
    const ui = this.ui,
      rows = this.rows(),
      final = !!this.session.finalResults;
    c.fillStyle = 'rgba(13,58,79,.9)';
    c.fillRect(50, 57, 540, 329);
    c.strokeStyle = '#6698a9';
    c.lineWidth = 3;
    c.strokeRect(53, 82, 534, 303);
    ui.sprite('OV_1-6', 0, 226, 256, 30, 330, 80, 256, 46);
    ui.text(c, ui.course?.label || 'Snow Jam - Race', 88, 88, 21, '#d1e1e2');
    ui.text(c, 'Single Event Results', 88, 112, 19, '#66a9bb');
    ui.text(c, 'Rank', 118, 149, 17, '#d1e1e2');
    ui.text(c, 'Riders', 198, 149, 17, '#d1e1e2');
    ui.text(c, 'Time', 418, 149, 17, '#d1e1e2');
    rows.forEach((r, i) => {
      const color = r.human ? '#e8bd72' : '#d1e1e2',
        y = 175 + i * 19;
      ui.text(c, r.dnf ? '-' : ORDINAL[i] || String(i + 1), 128, y, 18, color);
      ui.text(c, r.name || '', 198, y, 18, color);
      ui.text(c, r.dnf ? 'DNF' : raceTime(r.ticks, false), 418, y, 18, color);
    });
    if (!final) ui.text(c, 'Racers still on course...', 88, 175 + 6 * 19 + 4, 14, '#66a9bb');
    ui.items().forEach((label, i) => ui.text(c, label, 450, 299 + i * 15, 14, '#dce5e4'));
    ui.sprite('OV_1-2', 55, 122, 24, 24, 430, 313, 17, 17);
  }
  noticeText() {
    return performance.now() < this.noticeUntil ? this.notice : '';
  }
}

