import { useState } from 'react';
import pickle from '../assets/pickle.png';
import OccupancyMonitor from '../components/OccupancyMonitor';
import type { CourtInfo } from '../components/OccupancyMonitor';
import LiveStream from '../components/LiveStream';
import { ref, onValue, update } from 'firebase/database';
import { db } from '../firebase';

let listenerAttached = false;

export default function HomePage() {
  const [courts, setCourts] = useState<CourtInfo[]>([]);
  const [totalPlayers, setTotalPlayers] = useState(0);

  // Attach Firebase listener once
  if (!listenerAttached) {
    listenerAttached = true;

    const rootRef = ref(db);
    onValue(rootRef, snapshot => {
      if (!snapshot.exists()) {
        return;
      }

      const data = snapshot.val();
      const c1 = data.court1;
      const c2 = data.court2;

      // Court 1
      let c1Color = 'gray';
      let c1Status = 'available';

      if (c1.occupancyCount == 0) {
        c1Color = 'green';
        c1Status = 'available';
      } else if (c1.occupancyCount > 0 && c1.queueSize === 0) {
        c1Color = 'yellow';
        c1Status = 'occupied_no_queue';
      } else if (c1.occupancyCount > 0 && c1.queueSize > 0) {
        c1Color = 'red';
        c1Status = 'occupied_with_queue';
      }
      update(ref(db, 'court1'), {
        lightColor: c1Color,
        status: c1Status
      });

      // Court 2
      let c2Color = 'gray';
      let c2Status = 'available';
      if (c2.occupancyCount == 0) {
        c2Color = 'green';
        c2Status = 'available';
      } else if (c2.occupancyCount > 0 && c2.queueSize === 0) {
        c2Color = 'yellow';
        c2Status = 'occupied_no_queue';
      } else if (c2.occupancyCount > 0 && c2.queueSize > 0) {
        c2Color = 'red';
        c2Status = 'occupied_with_queue';
      }

      update(ref(db, 'court2'), {
        lightColor: c2Color,
        status: c2Status
      });

      // --- Update frontend ---
      const updatedCourts: CourtInfo[] = [
        {
          name: 'Court 1',
          players: c1.occupancyCount,
          queue: c1.queueSize,
          status: c1Status as CourtInfo['status'],
          lightColor: c1Color
        },
        {
          name: 'Court 2',
          players: c2.occupancyCount,
          queue: c2.queueSize,
          status: c2Status as CourtInfo['status'],
          lightColor: c2Color
        }
      ];

      setCourts(updatedCourts);
      setTotalPlayers(c1.occupancyCount + c2.occupancyCount + c1.queueSize + c2.queueSize);
    });
  }

  return (
    <>
      <section className='hero'>
        <div className='hero-title'>
          <img className='hero-logo' src={pickle} alt='pickleball paddles' />
          <div>
            <h1 className='site-title'>PicklePortal</h1>
            <p className='hero-tagline'>
              Plan your next pickleball match effortlessly.
            </p>
          </div>
        </div>
      </section>
      <div className='divider'></div>
      <OccupancyMonitor totalPlayers={totalPlayers} courts={courts} />
      <div className='divider'></div>
      <LiveStream />
    </>
  );
}
