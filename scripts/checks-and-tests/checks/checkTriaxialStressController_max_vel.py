# defining wall boundaries for an empty box:
wallX1id = O.bodies.append(wall( (0.2,0,0),0))
wallX2id = O.bodies.append(wall( (0.4,0,0),0))
wallY1id = O.bodies.append(wall( (0,0.2,0),1))
wallY2id = O.bodies.append(wall( (0,0.4,0),1))
wallZ1id = O.bodies.append(wall( (0,0,0.2),2))
wallZ2id = O.bodies.append(wall( (0,0,0.4),2))

# engines list:
O.dt=1.e-3
O.engines=[ForceResetter()
           ,InsertionSortCollider([Bo1_Wall_Aabb()])
           ,InteractionLoop(
               [Ig2_Wall_Sphere_ScGeom()],
               [Ip2_FrictMat_FrictMat_FrictPhys()],
               [Law2_ScGeom_FrictPhys_CundallStrack()]
                           )
           ,TriaxialStressController(internalCompaction = 0
                                     ,wall_bottom_id=wallY1id,wall_top_id=wallY2id
                                     ,wall_back_id=wallZ1id,wall_front_id=wallZ2id
                                     ,wall_left_id=wallX1id,wall_right_id=wallX2id
                                     ,stressMask=7,max_vel = 0.05 # NB: script behavior is here unaffected by stressDamping (walls do not oscillate anyway)
                                     ,thickness = 0
                                     ,label = 'tsc')
           ,NewtonIntegrator()
          ]

# searching for compression (or max_vel)
tsc.goal1 = tsc.goal2 = tsc.goal3 = - 20.e3
O.run(1000,True)

max_vel_result = max([abs(b.state.vel[axis]) for axis in range(3) for b in O.bodies])
print('\nAfter',O.iter,'iterations, we had a change in length =',tsc.width-tsc.width0,'vs',-2*tsc.max_vel*O.dt*O.iter,'expected.')
print('Maximum boundaries velocity is indeed',max_vel_result,'vs',tsc.max_vel,'expected\n')

if abs(max_vel_result - tsc.max_vel) > tsc.max_vel * 1e-3: 
	raise YadeCheckError("Maximum boundaries velocity differs from allowed max_vel by more than 0.1%.")
