import { Component, OnInit, NgZone, ChangeDetectorRef } from '@angular/core';
import { Database, ref, onValue, update } from '@angular/fire/database';
import { CommonModule } from '@angular/common';

interface HistoryItem {
  moisture: number;
  recommendation: string;
  timestamp: string;
  color: string;
}

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent implements OnInit {
  moisture = 0;
  moisturePercent = 0;
  recommendation = 'UČITAVANJE...';
  status = 'offline';
  lastUpdate = '';
  servoPosition = 90;
  manualActive = false;
  showWarning = false;
  history: HistoryItem[] = [];
  private lastToggleTime = 0;

  constructor(
    private db: Database,
    private ngZone: NgZone,
    private cdr: ChangeDetectorRef
  ) { }

  ngOnInit() {
    this.listenToData();
    this.listenToStatus();
  }

  listenToData() {
    const dataRef = ref(this.db, 'devices/ESP32-001/currentData');

    onValue(dataRef, (snapshot) => {
      this.ngZone.run(() => {
        const data = snapshot.val();
        if (data) {
          this.moisture = data.moisture || 0;
          this.moisturePercent = data.moisture || 0;
          this.recommendation = data.recommendation || 'UNKNOWN';
          this.servoPosition = data.servoPosition || 0;

          // Ažuriranje manualStanstanja - lock 5 sekundi
          const now = Date.now();
          if (now - this.lastToggleTime > 5000) {
            if (data.manualActive !== undefined) {
              this.manualActive = data.manualActive;
            }
          }

          // LOGIKA UPOZORENJA: NOVI PRAG 20%
          this.showWarning = data.warning || (this.manualActive && this.moisturePercent > 20);

          this.lastUpdate = new Date().toLocaleTimeString('bs-BA', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: false
          });

          this.addToHistory(data);
          this.cdr.detectChanges();
        }
      });
    });
  }

  listenToStatus() {
    const statusRef = ref(this.db, 'devices/ESP32-001/status');
    onValue(statusRef, (snapshot) => {
      this.ngZone.run(() => {
        this.status = snapshot.val() || 'offline';
        this.cdr.detectChanges();
      });
    });
  }

  addToHistory(data: any) {
    const recValue = data.recommendation || 'UNKNOWN';
    const isDuplicate = this.history.length > 0 &&
      this.history[0].moisture === data.moisture &&
      this.history[0].recommendation === recValue;

    if (!isDuplicate) {
      const item: HistoryItem = {
        moisture: data.moisture,
        recommendation: recValue,
        timestamp: new Date().toLocaleTimeString('bs-BA', {
          hour: '2-digit',
          minute: '2-digit',
          second: '2-digit',
          hour12: false
        }),
        color: this.getColorForRecommendation(recValue)
      };

      this.history.unshift(item);
      if (this.history.length > 5) {
        this.history = this.history.slice(0, 5);
      }
    }
  }

  toggleManualWater() {
    this.lastToggleTime = Date.now();
    this.manualActive = !this.manualActive;

    // Odmah ažuriraj upozorenje: NOVI PRAG 20%
    this.showWarning = (this.manualActive && this.moisturePercent > 20);

    // Slanje komande u Firebase
    const updates: any = {};
    updates['/devices/ESP32-001/commands/manualWater'] = this.manualActive;
    update(ref(this.db), updates);
  }

  getStateColor(): string {
    return this.getColorForRecommendation(this.recommendation);
  }

  getColorForRecommendation(rec: string): string {
    switch (rec) {
      case 'SUHO':
        return '#FF6B6B'; // Crvena
      case 'OPTIMALNO':
        return '#51CF66'; // Zelena
      case 'VLAZNO':
      case 'VLAŽNO':
        return '#4DABF7'; // Plava
      default:
        return '#ADB5BD'; // Siva
    }
  }

  getRecommendationText(): string {
    switch (this.recommendation) {
      case 'SUHO':
        return 'Potrebno Zalijevanje!';
      case 'OPTIMALNO':
        return 'Vlažnost je Idealna';
      case 'VLAZNO':
      case 'VLAŽNO':
        return 'Zemlja je Natopljena';
      default:
        return this.recommendation;
    }
  }

  getServoStatus(): string {
    if (this.servoPosition >= 160) return 'Ventil Otvoren';
    if (this.servoPosition >= 80 && this.servoPosition <= 100) return 'Polu-otvoreno';
    if (this.servoPosition <= 20) return 'Ventil Zatvoren';
    return 'Pomjeranje...';
  }

  getServoColor(): string {
    if (this.servoPosition >= 160) return '#FF6B6B';
    if (this.servoPosition >= 80 && this.servoPosition <= 100) return '#FFB347';
    return '#4ECDC4';
  }
}