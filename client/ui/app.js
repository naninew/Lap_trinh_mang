/* NetChess Minimalist UI Controller (Zero Dependency, Lichess Style) */
const PIECE_GLYPHS = {
    1: '♟', 2: '♞', 3: '♝', 4: '♜', 5: '♛', 6: '♚',
    '-1': '♟', '-2': '♞', '-3': '♝', '-4': '♜', '-5': '♛', '-6': '♚'
};

const PIECE_VALUES = { 1: 1, 2: 3, 3: 3, 4: 5, 5: 9, 6: 0 };

let ws = null;
let myUser = '';
let myScore = 100;
let myRank = 1;
let myColor = 'white';
let currentTurn = 'white';
let selectedSq = -1;
let lastMoveFrom = -1;
let lastMoveTo = -1;
let validMoves = [];
let pendingChallenger = '';
let challengeTargetUser = '';
let lastMatchLog = '';
let lastLogFilename = 'match.log';
let savedUser = '';
let savedPass = '';
let lobbyRefreshTimer = null;
let clockTimer = null;
let whiteTime = 600;
let blackTime = 600;
let isKicked = false;
let isReviewMode = false;
let isKingInCheck = false;
let moveHistoryCount = 0;
let pendingAuthPayload = null;

/* Initial standard chess board 8x8 */
let boardState = [
     4,  2,  3,  5,  6,  3,  2,  4,
     1,  1,  1,  1,  1,  1,  1,  1,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -4, -2, -3, -5, -6, -3, -2, -4
];

function getServerHost() {
    const input = document.getElementById('serverHost');
    if (input && input.value.trim()) {
        return input.value.trim();
    }
    return window.location.hostname ? `${window.location.hostname}:8888` : 'localhost:8888';
}

function connectSocket() {
    if (isKicked) return;
    const host = getServerHost();
    const wsUrl = `ws://${host}`;
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        document.getElementById('statusDot').className = 'status-dot connected';
        document.getElementById('statusText').innerText = `Connected (${host})`;
        if (pendingAuthPayload) {
            ws.send(JSON.stringify(pendingAuthPayload));
            pendingAuthPayload = null;
        } else if (savedUser && savedPass) {
            ws.send(JSON.stringify({ action: 'LOGIN', user: savedUser, pass: savedPass }));
        }
    };

    ws.onclose = () => {
        document.getElementById('statusDot').className = 'status-dot disconnected';
        document.getElementById('statusText').innerText = 'Disconnected. Retrying...';
        if (isKicked) return;
        setTimeout(connectSocket, 2000);
    };

    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            handleServerEvent(data);
        } catch (e) {
            console.error('Failed to parse frame:', event.data);
        }
    };
}

function startLobbyRefresh() {
    if (lobbyRefreshTimer) clearInterval(lobbyRefreshTimer);
    lobbyRefreshTimer = setInterval(() => {
        const lobby = document.getElementById('lobbySection');
        if (lobby && lobby.style.display !== 'none' && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ action: 'IDLE_LIST_REQ' }));
        }
    }, 3000);
}

function startClockTimer() {
    if (clockTimer) clearInterval(clockTimer);
    clockTimer = setInterval(() => {
        if (currentTurn === 'white') {
            if (whiteTime > 0) whiteTime--;
        } else {
            if (blackTime > 0) blackTime--;
        }
        renderClocks();
    }, 1000);
}

function formatTime(sec) {
    if (sec < 0) sec = 0;
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
}

function renderClocks() {
    const oppClockEl = document.getElementById('oppClock');
    const myClockEl = document.getElementById('myClock');
    if (!oppClockEl || !myClockEl) return;

    const myT = (myColor === 'white') ? whiteTime : blackTime;
    const oppT = (myColor === 'white') ? blackTime : whiteTime;

    myClockEl.innerText = formatTime(myT);
    oppClockEl.innerText = formatTime(oppT);

    myClockEl.classList.toggle('active-clock', currentTurn === myColor);
    oppClockEl.classList.toggle('active-clock', currentTurn !== myColor);

    myClockEl.classList.toggle('low-time', myT <= 30);
    oppClockEl.classList.toggle('low-time', oppT <= 30);
}

function getPieceSvg(p) {
    if (p === 0) return '';
    const isWhite = (p > 0);
    const glyph = PIECE_GLYPHS[p] || '';
    if (isWhite) {
        return `<svg viewBox="0 0 100 100">
            <text x="50" y="80" text-anchor="middle" font-size="78"
                  font-family="'Segoe UI Symbol', 'DejaVu Sans', 'Apple Color Emoji', sans-serif"
                  fill="#ffffff" stroke="#262421" stroke-width="4.5" stroke-linejoin="round"
                  paint-order="stroke fill">${glyph}</text>
        </svg>`;
    } else {
        return `<svg viewBox="0 0 100 100">
            <text x="50" y="80" text-anchor="middle" font-size="78"
                  font-family="'Segoe UI Symbol', 'DejaVu Sans', 'Apple Color Emoji', sans-serif"
                  fill="#262421" stroke="#ffffff" stroke-width="1.2" stroke-linejoin="round"
                  paint-order="stroke fill">${glyph}</text>
        </svg>`;
    }
}

function sqToAlg(sq) {
    const f = sq % 8;
    const r = Math.floor(sq / 8);
    const fileChar = String.fromCharCode(97 + f);
    return `${fileChar}${r + 1}`;
}

function handleServerEvent(data) {
    switch (data.action) {
        case 'REGISTER_RESP':
            showAlert(data.msg, data.status === 'OK' ? 'success' : 'error');
            break;
        case 'LOGIN_RESP':
            if (data.status === 'OK') {
                myUser = data.user;
                myScore = data.score;
                myRank = data.rank;
                isKicked = false;
                document.getElementById('myUsername').innerText = myUser;
                document.getElementById('myScore').innerText = myScore + ' pts';
                document.getElementById('myRank').innerText = 'Rank #' + myRank;
                document.getElementById('authSection').style.display = 'none';

                if (document.getElementById('matchSection').style.display !== 'none') {
                    document.getElementById('matchSection').style.display = 'none';
                    alert('The server has restarted. Previous match ended. Returned to lobby.');
                }
                document.getElementById('lobbySection').style.display = 'block';
                startLobbyRefresh();
                ws.send(JSON.stringify({ action: 'IDLE_LIST_REQ' }));
            } else {
                showAlert(data.msg, 'error');
            }
            break;
        case 'KICKED':
            isKicked = true;
            savedUser = '';
            savedPass = '';
            pendingAuthPayload = null;
            if (lobbyRefreshTimer) clearInterval(lobbyRefreshTimer);
            if (clockTimer) clearInterval(clockTimer);
            document.getElementById('matchSection').style.display = 'none';
            document.getElementById('lobbySection').style.display = 'none';
            document.getElementById('authSection').style.display = 'block';
            showAlert(data.reason || 'Account signed in from another device or session.', 'error');
            setTimeout(() => {
                isKicked = false;
                connectSocket();
            }, 300);
            break;
        case 'IDLE_LIST':
            renderIdleList(data.players || []);
            break;
        case 'CHALLENGE_ERR':
            alert(data.msg);
            break;
        case 'CHALLENGE_NOTIFY':
            pendingChallenger = data.from;
            const modeName = (data.time_mode === 60) ? 'Classical (60 mins)' : 'Rapid (10 mins)';
            document.getElementById('challengePromptText').innerHTML =
                `Player <strong>${data.from}</strong> (Rank #${data.rank}, ${data.score} pts) challenged you to a <strong>${modeName}</strong> match.`;
            document.getElementById('challengeModal').style.display = 'flex';
            break;
        case 'GAME_START':
            if (lobbyRefreshTimer) clearInterval(lobbyRefreshTimer);
            myColor = data.color;
            currentTurn = 'white';
            selectedSq = -1;
            lastMoveFrom = -1;
            lastMoveTo = -1;
            validMoves = [];
            moveHistoryCount = 0;
            isReviewMode = false;
            isKingInCheck = false;
            whiteTime = data.time_control || 600;
            blackTime = data.time_control || 600;

            initBoardState();
            document.getElementById('lobbySection').style.display = 'none';
            document.getElementById('matchSection').style.display = 'flex';
            document.getElementById('btnResign').style.display = 'inline-block';
            document.getElementById('btnReviewReturnToLobby').style.display = 'none';

            document.getElementById('oppName').innerText = data.opp_name;
            document.getElementById('oppScore').innerText = data.opp_score + ' pts';
            document.getElementById('boardMyName').innerText = myUser;
            document.getElementById('boardMyScore').innerText = myScore + ' pts';
            document.getElementById('liveMoveBody').innerHTML = '';
            document.getElementById('oppCapturedTray').innerHTML = '';
            document.getElementById('myCapturedTray').innerHTML = '';

            updateTurnBadge();
            renderClocks();
            renderBoard();
            startClockTimer();
            break;
        case 'VALID_MOVES':
            validMoves = data.dests || [];
            renderBoard();
            break;
        case 'MOVE_BROADCAST':
            if (data.board && Array.isArray(data.board) && data.board.length === 64) {
                boardState = data.board;
            } else {
                applyMoveLocally(data.from, data.to, data.promo);
            }
            currentTurn = data.turn;
            lastMoveFrom = data.from;
            lastMoveTo = data.to;
            selectedSq = -1;
            validMoves = [];
            isKingInCheck = (data.check === true);

            if (data.w_time !== undefined) whiteTime = data.w_time;
            if (data.b_time !== undefined) blackTime = data.b_time;

            renderCapturedTray('myCapturedTray', (myColor === 'white') ? (data.w_caps || []) : (data.b_caps || []), myColor === 'white');
            renderCapturedTray('oppCapturedTray', (myColor === 'white') ? (data.b_caps || []) : (data.w_caps || []), myColor !== 'white');

            appendLiveMove(data.from, data.to);
            updateTurnBadge();
            renderClocks();
            renderBoard();
            break;
        case 'MOVE_REJECT':
            alert('Illegal move: ' + data.msg);
            selectedSq = -1;
            validMoves = [];
            renderBoard();
            break;
        case 'GAME_OVER':
            if (clockTimer) clearInterval(clockTimer);
            document.getElementById('gameOverTitle').innerText =
                (data.winner === myUser) ? '🎉 You WON (+3 pts)!' : 'Game Over!';
            document.getElementById('gameOverSummary').innerText =
                `Winner: ${data.winner} (${data.reason}). Score change: ${data.score_delta > 0 ? '+' : ''}${data.score_delta} pts`;
            document.getElementById('gameOverModal').style.display = 'flex';
            break;
        case 'LOG_FILE':
            lastLogFilename = data.filename || 'match.log';
            lastMatchLog = data.content || '';
            document.getElementById('logContent').innerText = lastMatchLog;
            break;
    }
}

function initBoardState() {
    boardState = [
         4,  2,  3,  5,  6,  3,  2,  4,
         1,  1,  1,  1,  1,  1,  1,  1,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
        -1, -1, -1, -1, -1, -1, -1, -1,
        -4, -2, -3, -5, -6, -3, -2, -4
    ];
}

function renderBoard() {
    const boardEl = document.getElementById('chessBoard');
    boardEl.innerHTML = '';

    /* Identify checked king position */
    let checkedKingSq = -1;
    if (isKingInCheck) {
        const targetKing = (currentTurn === 'white') ? 6 : -6;
        for (let i = 0; i < 64; i++) {
            if (boardState[i] === targetKing) {
                checkedKingSq = i;
                break;
            }
        }
    }

    for (let r = 7; r >= 0; r--) {
        const displayRank = (myColor === 'white') ? r : (7 - r);
        for (let f = 0; f < 8; f++) {
            const displayFile = (myColor === 'white') ? f : (7 - f);
            const sqIdx = displayRank * 8 + displayFile;

            const sqDiv = document.createElement('div');
            const isLight = (displayRank + displayFile) % 2 === 1;
            sqDiv.className = `square ${isLight ? 'light' : 'dark'}`;

            if (sqIdx === lastMoveFrom) {
                sqDiv.classList.add('prev-move-from');
            } else if (sqIdx === lastMoveTo) {
                sqDiv.classList.add('prev-move-to');
            }

            if (sqIdx === selectedSq) {
                sqDiv.classList.add('selected');
            }

            if (sqIdx === checkedKingSq) {
                sqDiv.classList.add('in-check');
            }

            if (validMoves.includes(sqIdx)) {
                if (boardState[sqIdx] !== 0) {
                    sqDiv.classList.add('valid-capture-hint');
                } else {
                    sqDiv.classList.add('valid-hint');
                }
            }

            const p = boardState[sqIdx];
            if (p !== 0) {
                const pSpan = document.createElement('span');
                pSpan.className = `piece ${p > 0 ? 'white' : 'black'}`;
                pSpan.innerHTML = getPieceSvg(p);
                sqDiv.appendChild(pSpan);
            }

            sqDiv.onclick = () => onSquareClicked(sqIdx);
            boardEl.appendChild(sqDiv);
        }
    }
}

function onSquareClicked(sq) {
    if (isReviewMode) return;
    if (currentTurn !== myColor) return;

    if (selectedSq === -1) {
        const p = boardState[sq];
        if (p === 0) return;
        if ((myColor === 'white' && p > 0) || (myColor === 'black' && p < 0)) {
            selectedSq = sq;
            validMoves = [];
            /* Request strictly legal moves from C server */
            ws.send(JSON.stringify({ action: 'GET_VALID_MOVES', sq: sq }));
            renderBoard();
        }
    } else {
        if (sq === selectedSq) {
            selectedSq = -1;
            validMoves = [];
            renderBoard();
            return;
        }
        /* Send move request to C server for 100% logic validation */
        ws.send(JSON.stringify({
            action: 'MOVE_REQ',
            from: selectedSq,
            to: sq,
            promo: 0
        }));
        selectedSq = -1;
        validMoves = [];
        renderBoard();
    }
}

function applyMoveLocally(from, to, promo) {
    const p = boardState[from];
    boardState[from] = 0;
    const toRank = Math.floor(to / 8);

    /* Castling rook update */
    if (Math.abs(p) === 6 && Math.abs((to % 8) - (from % 8)) === 2) {
        if (to === 6) { boardState[7] = 0; boardState[5] = 4; }
        else if (to === 2) { boardState[0] = 0; boardState[3] = 4; }
        else if (to === 62) { boardState[63] = 0; boardState[61] = -4; }
        else if (to === 58) { boardState[56] = 0; boardState[59] = -4; }
    }

    /* Pawn promotion */
    if (Math.abs(p) === 1 && (toRank === 7 || toRank === 0)) {
        const promoPiece = (promo !== undefined && promo !== 0) ? Math.abs(promo) : 5;
        boardState[to] = (p > 0) ? promoPiece : -promoPiece;
    } else {
        boardState[to] = p;
    }
}

function updateTurnBadge() {
    const badge = document.getElementById('oppStatusBadge');
    if (!badge) return;
    if (currentTurn === myColor) {
        badge.innerText = 'Your Turn';
        badge.style.color = '#629924';
    } else {
        badge.innerText = "Opponent's Turn...";
        badge.style.color = '#888888';
    }
}

function renderCapturedTray(elementId, capturedArray, isCapturerWhite) {
    const tray = document.getElementById(elementId);
    if (!tray) return;
    tray.innerHTML = '';
    if (!capturedArray || capturedArray.length === 0) return;

    /* Count pieces by absolute piece type */
    const counts = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0 };
    capturedArray.forEach(p => {
        const absVal = Math.abs(p);
        if (counts[absVal] !== undefined) counts[absVal]++;
    });

    /* Sorted by value: Pawn (1) -> Knight (2) -> Bishop (3) -> Rook (4) -> Queen (5) */
    [1, 2, 3, 4, 5].forEach(type => {
        const n = counts[type];
        if (n > 0) {
            const grp = document.createElement('span');
            grp.className = 'cap-group';
            const displayGlyph = PIECE_GLYPHS[isCapturerWhite ? -type : type] || '';
            grp.innerHTML = `<span class="cap-symbol">${displayGlyph}</span>${n > 1 ? `<span class="cap-num">x${n}</span>` : ''}`;
            tray.appendChild(grp);
        }
    });
}

function appendLiveMove(from, to) {
    const tbody = document.getElementById('liveMoveBody');
    if (!tbody) return;

    const notation = `${sqToAlg(from)}-${sqToAlg(to)}`;
    if (moveHistoryCount % 2 === 0) {
        /* New turn row */
        const turnNum = Math.floor(moveHistoryCount / 2) + 1;
        const tr = document.createElement('tr');
        tr.id = `moveRow_${turnNum}`;
        tr.innerHTML = `<td>${turnNum}.</td><td id="wMove_${turnNum}">${notation}</td><td id="bMove_${turnNum}">...</td>`;
        tbody.appendChild(tr);
    } else {
        /* Append Black move to existing row */
        const turnNum = Math.floor(moveHistoryCount / 2) + 1;
        const bCell = document.getElementById(`bMove_${turnNum}`);
        if (bCell) bCell.innerText = notation;
    }
    moveHistoryCount++;

    const wrapper = document.getElementById('liveMoveWrapper');
    if (wrapper) wrapper.scrollTop = wrapper.scrollHeight;
}

function renderIdleList(players) {
    const tbody = document.getElementById('idlePlayerList');
    tbody.innerHTML = '';

    if (!players || players.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="text-center">No other players found.</td></tr>';
        return;
    }

    players.forEach(p => {
        const tr = document.createElement('tr');
        const isOnline = (p.online === 1 || p.online === true);
        const canChallenge = (p.can_challenge === 1 || p.can_challenge === true);
        const statusText = p.status_text || (isOnline ? (canChallenge ? 'Ready to Challenge' : 'Not Eligible') : 'Offline');

        if (!isOnline) {
            tr.classList.add('offline-row');
        }

        tr.innerHTML = `
            <td><strong>#${p.rank}</strong></td>
            <td>${p.user} ${isOnline ? '<span style="color:#629924; font-size:0.75rem;">●</span>' : '<span style="color:#aaa; font-size:0.75rem;">○</span>'}</td>
            <td>${p.score} pts</td>
            <td>
                <button class="btn btn-sm ${canChallenge ? 'btn-primary' : 'btn-secondary'}"
                        onclick="promptChallenge('${p.user}')"
                        ${canChallenge ? '' : 'disabled'}
                        title="${statusText}">
                    ${canChallenge ? 'Challenge' : statusText}
                </button>
            </td>
        `;
        tbody.appendChild(tr);
    });
}

function promptChallenge(target) {
    challengeTargetUser = target;
    document.getElementById('targetPlayerName').innerText = target;
    document.getElementById('challengeModeModal').style.display = 'flex';
}

function sendChallenge(target, timeMode) {
    ws.send(JSON.stringify({ action: 'CHALLENGE_REQ', target: target, time_mode: timeMode }));
}

function returnToLobbyFromMatch() {
    isReviewMode = false;
    isKingInCheck = false;
    if (clockTimer) clearInterval(clockTimer);
    document.getElementById('gameOverModal').style.display = 'none';
    document.getElementById('matchSection').style.display = 'none';
    document.getElementById('lobbySection').style.display = 'block';
    document.getElementById('btnResign').style.display = 'inline-block';
    document.getElementById('btnReviewReturnToLobby').style.display = 'none';
    startLobbyRefresh();
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'IDLE_LIST_REQ' }));
    }
}

function showAlert(msg, type) {
    const alertEl = document.getElementById('authAlert');
    alertEl.className = 'alert-box ' + type;
    alertEl.innerText = msg;
    alertEl.style.display = 'block';
}

function sendOrConnectAuth(action, u, p) {
    savedUser = u;
    savedPass = p;
    isKicked = false;

    if (!ws || ws.readyState !== WebSocket.OPEN) {
        pendingAuthPayload = { action: action, user: u, pass: p };
        connectSocket();
        return;
    }

    ws.send(JSON.stringify({ action: action, user: u, pass: p }));
}

/* Event Listeners */
document.getElementById('btnLogin').onclick = () => {
    const u = document.getElementById('username').value.trim();
    const p = document.getElementById('password').value.trim();
    if (!u || !p) return showAlert('Please enter both username and password', 'error');
    sendOrConnectAuth('LOGIN', u, p);
};

document.getElementById('btnRegister').onclick = () => {
    const u = document.getElementById('username').value.trim();
    const p = document.getElementById('password').value.trim();
    if (!u || !p) return showAlert('Please enter both username and password', 'error');
    sendOrConnectAuth('REGISTER', u, p);
};

document.getElementById('btnChangeHost').onclick = () => {
    if (ws) {
        ws.close();
    }
    connectSocket();
};

document.getElementById('btnRefreshLobby').onclick = () => {
    ws.send(JSON.stringify({ action: 'IDLE_LIST_REQ' }));
};

document.getElementById('btnExitLobby').onclick = () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'LOGOUT' }));
    }
    savedUser = '';
    savedPass = '';
    pendingAuthPayload = null;
    isKicked = false;
    if (lobbyRefreshTimer) clearInterval(lobbyRefreshTimer);
    if (clockTimer) clearInterval(clockTimer);
    document.getElementById('matchSection').style.display = 'none';
    document.getElementById('lobbySection').style.display = 'none';
    document.getElementById('authSection').style.display = 'block';
    showAlert('Signed out successfully.', 'success');
};

document.getElementById('btnModeRapid').onclick = () => {
    document.getElementById('challengeModeModal').style.display = 'none';
    sendChallenge(challengeTargetUser, 10);
};

document.getElementById('btnModeStandard').onclick = () => {
    document.getElementById('challengeModeModal').style.display = 'none';
    sendChallenge(challengeTargetUser, 60);
};

document.getElementById('btnCancelChallengeMode').onclick = () => {
    document.getElementById('challengeModeModal').style.display = 'none';
};

document.getElementById('btnAcceptChallenge').onclick = () => {
    document.getElementById('challengeModal').style.display = 'none';
    ws.send(JSON.stringify({ action: 'CHALLENGE_REPLY', from: pendingChallenger, accept: true }));
};

document.getElementById('btnDeclineChallenge').onclick = () => {
    document.getElementById('challengeModal').style.display = 'none';
    ws.send(JSON.stringify({ action: 'CHALLENGE_REPLY', from: pendingChallenger, accept: false }));
};

document.getElementById('btnResign').onclick = () => {
    if (confirm('Are you sure you want to resign the game? (Will be scored as a loss -3 pts)')) {
        ws.send(JSON.stringify({ action: 'RESIGN' }));
    }
};

document.getElementById('btnReviewBoard').onclick = () => {
    document.getElementById('gameOverModal').style.display = 'none';
    isReviewMode = true;
    document.getElementById('btnResign').style.display = 'none';
    document.getElementById('btnReviewReturnToLobby').style.display = 'inline-block';
};

document.getElementById('btnReviewReturnToLobby').onclick = returnToLobbyFromMatch;

document.getElementById('btnCloseGameOver').onclick = returnToLobbyFromMatch;

document.getElementById('btnDownloadLog').onclick = () => {
    const blob = new Blob([lastMatchLog], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = lastLogFilename;
    a.click();
    URL.revokeObjectURL(url);
};

window.onload = connectSocket;