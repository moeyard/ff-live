import React from 'react';
import ReactDOM from 'react-dom';
import {Provider,connect} from  'react-redux';
import thunk from  'redux-thunk';
import {createStore,bindActionCreators, applyMiddleware} from 'redux';
import "./app.scss";
import { HashRouter as Router, Route , withRouter} from 'react-router-dom'
import {Input,Button,Layout,Row,Col} from 'antd';
import 'antd/dist/antd.css';
const {Header,Footer,Sider,Content} = Layout;
const server = 'localhost';

class MyApp extends React.Component {
  render(){
    let state = this.props.state;
    let actions = this.props.actions;
    return  (
      <div>
      <Row>
      <video></video>
      </Row>
      <Row>
      <Button size='large' onClick={actions.connect}>{'connect'}</Button>
      <Button size='large' onClick={actions.disconnect }>{'disconnect'}</Button>
      </Row>
      </div>
    );
  } 
}
const InitState = {
  connected : false
}
var ws;
var mediaSource;
const Reducer = (state=InitState, action) => {
  let NewState = {
    connected : state.connected
  }
  switch(action.type){
    default:
      return NewState;
  }
};
const store = createStore(Reducer, applyMiddleware(thunk));
const MapState = state =>({
  state: state
});
let actions = {
   connect:()=>((dispatch,getState)=> {
        var mediaSource = new MediaSource;
        var video = document.querySelector('video');
        mediaSource.addEventListener('sourceopen', openSource);
        video.src = URL.createObjectURL(mediaSource);
        function openSource(e){
          const mimeCodec = 'video/mp4; codecs="avc1.42E01E, mp4a.40.2"';
	  var mediaSource = this;
          var sourceBuffer = mediaSource.addSourceBuffer(mimeCodec);
  	  ws = new WebSocket('ws://'+server+':8080/stream');
          ws.binaryType = 'arraybuffer';
	  ws.addEventListener('message',(e)=> {
	    sourceBuffer.appendBuffer(e.data);
	  });
          sourceBuffer.addEventListener('updateend', function (_) {
            video.play();
          });
        };	
  }),
  disconnect:()=>((dispatch,getState)=> {
    ws.close();
  })
}
const MapDispatch = dispatch => ({
  actions: bindActionCreators(actions, dispatch)
});
let MyAppConnected = withRouter(connect(MapState,MapDispatch)(MyApp));
ReactDOM.render(
  <div>
  <Provider store={store}>
  <Router>
  <div>
  <Route exact path="/" component={MyAppConnected}/>
  </div>
  </Router>
  </Provider>
  </div>,
  document.getElementById('app')
);

